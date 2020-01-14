```
SNUCSE operating system 2019 Project4: Geo-tagged File System
```

# Overview: Geo-tagged File System
이번 프로젝트는 기본 ext2 파일 시스템에 gps_location(위도, 경도)를 추가하는 것이 목적이다. 파일이 생성 및 수정될 때 파일의 inode에는 생성 시점의 위도, 경도, 정확도 정보가 들어가게 된다. 그리고 생성된 파일에 다시 접근하려하면 현재 접근 시점의 위치 정보와 파일 inode의 위치 정보를 비교하여 접근 가능한 가까운 위치의 경우엔 접근을 허용하고, 그렇지 않으면 permission denied 시킨다.

# File System Modification

### 1. e2fsprogs/lib/ext2fs/ext2_fs.h
ext2 파일 시스템을 생성해주는 `mke2fs`를 사용하기에 앞서서 ext2 inode 구조체에 gps_location 정보가 들어가도록 수정을 해야한다. `ext2_inode` 구조체에 다음 내용을 추가해준다.

```
__u32	i_lat_integer;
__u32	i_lat_fractional;
__u32	i_lng_integer;
__u32	i_lng_fractional;
__u32	i_accuracy;
```

### 2. fs/ext2/ext2.h
앞에서 `ext2_fs.h`를 수정한 것과 마찬가지로 `ext2_inode` 구조체에 동일한 형식으로 gps_location 정보를 넣어준다. 이 때 주의할 점은 `ext2_inode`(inode in disk)와 `ext2_inode_info`(inode in memory)에 모두 gps_location 정보를 넣어주어야 한다는 것이다.

### 3. fs/ext2/file.c
> 이 소스파일을 처음 접했을 때 어느 부분을 수정할지 잘 몰라서 어려움이 있었다. 일단 참고할 수 있는 소스 중엔 ext4의 `file.c`가 있어서 해당 파일의 함수들을 많이 참고하였다.

#### gps_location write
- 가장 중요한 부분은 `static ssize_t ext2_file_write_iter(struct kiocb *iocb, struct iov_iter *from)` 함수이다.
- 이는 파일 write를 할 때 호출이 되는 함수로 우리는 여기에 gps_location을 write하는 기능을 추가해야한다.
- 아래와 같이 `file_inode` 함수를 통해 파일의 inode 정보를 얻고, `set_gps_location` 함수를 통해서 얻은 inode의 gps_location 정보를 세팅한다.

```
static ssize_t ext2_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
    // get inode of the file
    struct inode *inode = file_inode(iocb->ki_filp);

    if(inode->i_op->set_gps_location) {
        inode->i_op->set_gps_location(inode);
    }

    ...
}
```

- 이 함수는 아래와 같이 `const struct file_operations ext2_file_operations`을 통해서 참조된다.

```
const struct file_operations ext2_file_operations = {
	.llseek		= generic_file_llseek,
	.read_iter	= ext2_file_read_iter,
	.write_iter	= ext2_file_write_iter,     // ext_file_write_iter need to be upgraded
	.unlocked_ioctl = ext2_ioctl,
  
  ...
  
}
```

#### permission check

- 그 다음으로 구현할 부분은 파일에 접근할 때마다 접근 권한을 체크하는 부분이다.
- 아래와 같은 식으로 permission을 확인한다. 이 때 `nearby` 함수는 inode의 위치를 현재 위치와 비교해서 접근권한을 체크하는 함수인데, 이는 `kernel/gps.c`에 구현되어있다.

```
int ext2_perm(struct inode * inode, int mask){
	int ret;
	if((ret=generic_permission(inode, mask))!=0){
		return ret;
	}

	if(nearby(inode)){
		return -EACCES;
	}
	return ret;
}
```

- 마찬가지로 `inode_operations ext2_file_inode_operations`에서 참조되고 있다.

### 4. fs/ext2/inode.c

- `int ext2_set_gps_location(struct inode *inode)`, `int ext2_get_gps_location(struct inode *inode, struct gps_location *loc)`와 같이 파일 시스템 상에서 gps_location을 핸들링하는 함수들을 구현한 파일이다.

### 5. fs/ext2/namei.c
- 파일 생성시 inode에 gps_location 정보를 자동으로 추가하는 기능을 넣어준다.
- `static int ext2_create (struct inode * dir, struct dentry * dentry, umode_t mode, bool excl)` 함수 내에서 다음과 같이 파일 inode에 대하여 gps_location 정보를 세팅하도록 한다.

```
if(inode->i_op->set_gps_location){
    inode->i_op->set_gps_location(inode);
}
```

# Data Strcuture

중요한 자료구조는 크게 두 가지가 있다.

### 1. gps_location
```
struct gps_location {
    int lat_integer;
    int lat_fractional;
    int lng_integer;
    int lng_fractional;
    int accuracy;
};
```

- `/include/linux/gps.h`에 정의되어 있으며, 기본적인 gps_location 정보들을 모두 포함한다.

### 2. gps_fixp
```
typedef struct gps_fixed_point {
    int integer;
    int fraction;
} gps_fixp;
```

- 커널에는 `float` 타입이 없기 때문에 소수 연산을 위해서는 정수부와 소수부를 따로 고려하여 계산을 해야한다.
- 사칙연산, 삼각함수 등을 하나하나 직접 구현해야했기 때문에 이 부분에 어려움이 있었다. 


# System Call

### 1. sys_set_gps_location
- `long sys_set_gps_location(struct gps_location __user *loc)`
- `/kernel/gps.c`에는 `latest_location`이라는 gps_location이 전역 변수로 선언되어있고, 이것이 유저가 파일에 접근하는 시점의 위치 정보라고 생각하면 된다.
- 이 시스템 콜에서는 user space에서 할당된 gps_location 정보를 `copy_from_user` 하여 해당 gps_location 정보를 `latest_location`에 세팅해주는 기능을 한다.

### 2. sys_get_gps_location
- `long sys_get_gps_location(const char __user *pathname, struct gps_location __user *loc)`
- 이 시스템 콜을 user space로 부터 pathname을 `copy_from_user`로 받아서 **현재 위치가 접근 가능 위치라면**, 해당 pathname에 해당하는 파일의 inode의 gps_location을 추출하고, 그것을 `loc`을 통해서 `copy_to_user` 해준다.


# Mathematics for Calculation

현재 위치가 접근 가능한 위치인지 계산을 하기 위해서는 `gps_fixp` 구조체로 정의되는 위도, 경도 정보에 대한 산술 연산이 필요하다. 다음은 우리가 접근 가능 여부를 계산하기위해 구현한 연산의 목록이다.

1. add
2. sub
3. mult
4. div (gps_fixp / int)
5. factorial (for taylor expansion)
6. gps_fixp_pow (for taylor expansion)
7. gps_deg2rad (각도 -> 라디안 변환)
8. sin
9. cos
10. acos (arc cosine)


## Distance Between Two GPS Locations

<img width="304" alt="스크린샷 2019-12-17 오전 12 15 51" src="https://user-images.githubusercontent.com/17061663/70918938-47ff8280-2063-11ea-834d-ea253d881f42.png">

<img width="342" alt="스크린샷 2019-12-17 오전 12 15 59" src="https://user-images.githubusercontent.com/17061663/70918932-459d2880-2063-11ea-9461-d375ba7678ba.png">

- 위의 식은 구면좌표계 상에서 두 점 사이의 거리를 측정하기 위한 공식이다.
- 위의 식을 통해서 cos(alpha)를 구하고 이에 arc cosine을 취하여 aplpha 값을 구한다.
- alpha 값에 지구의 반지름인 6371(km)를 곱하면 두 점 사이의 거리(km)를 구할 수 있다.
- 접근 가능한 위치인지 판별하는 방법은 다음과 같다.
   + 두 점의 gps_location의 accuracy를 각각 acc1, acc2라고 하고, 위에서 구한 두 점 사이의 거리를 미터로 변환한 것이 d라고 하자.
   + if (acc1 + acc2) - d >= 0: accessible
   + if (acc1 + acc2) - d < 0: not accessible


## sin(x) and cos(x)

<img width="213" alt="스크린샷 2019-12-17 오전 12 26 23" src="https://user-images.githubusercontent.com/17061663/70919249-e25fc600-2063-11ea-8de4-39701329e54f.png">

- 위의 식과 같이 taylor expansion을 활용하여 값을 근사한다.
- sin과 cos는 적은 iteration만으로도 상당히 정확한 값을 얻을 수 있었다. (총 6번의 iteration 사용)
   + 6번을 반복한 이유는 6번 정도 반복했을 때 값이 수렴하는 모습을 직접 검증했기 때문이다.


## acos(x)

> Arc Cosine의 구현은 이번 과제에서 가장 어려웠던 부분 중 하나였다.

<img width="346" alt="스크린샷 2019-12-17 오전 12 16 30" src="https://user-images.githubusercontent.com/17061663/70918923-42a23800-2063-11ea-9352-f5c7dcedfed9.png">

- arc cosine의 식은 위의 그림과 같다.
- arc cosine은 다른 삼각함수들과는 달리 실제 값과의 오차가 컸기 때문에 11차 항까지 넣어 계산을 했으며 최종 식은 위 식에 (63/2816*x^11)을 더한 값과 같다.


<img width="987" alt="스크린샷 2019-12-17 오전 12 13 39" src="https://user-images.githubusercontent.com/17061663/70918945-49c94600-2063-11ea-9157-e1d18756d652.png">

- 위의 표는 arccosine의 식과 실제 값과의 오차를 보기 위하여 구한 엑셀 표이다.
- 최대 오차는 0.25410486으로 이를 거리로 환산하면 최대 1618.902km가 차이가 난다.
- 오차의 값이 너무 클 뿐만이 아니라, 실제 나와야 하는 값이 0에 가까울 수록 오차가 커졌기 때문에 문제가 되었다.
- Permission은 거리가 주어진 accuracy보다 작거나 같아야 주어지는데, accuracy가 meter 단위였기 때문에 작은 값에서 큰 정확성을 보여야 했기 때문이다.
- 따라서 구간을 나누어 최대 오차가 0.1 이하가 되도록 구간을 나누어 bias를 주었다. 아래의 표는 bias를 넣지 않은 acos 결과와 bias를 넣은 acos 결과를 나타낸 표이다.

![os](https://user-images.githubusercontent.com/13301302/70931980-ffa08e80-207b-11ea-8085-0862c35586f9.PNG)

# Test Files

## 1. gpsupdate
- 유저로 부터 gps_location 정보를 하나씩 scanf로 입력 받고, 그 값을 gps_location 구조체에 저장한 뒤 `sys_set_gps_location` 시스템콜을 통해 현재 위치(`latest_location`)을 커널 메모리 상에 세팅한다.

## 2. file_loc
- argument를 통해서 file의 pathname을 입력받는다.
- pathname과 path의 gps_location을 커널로부터 카피해올 gps_location 구조체 변수를 가지고 `sys_get_gps_location` 함수를 호출한다.
- 만약 pathname에 해당하는 파일의 gps_location과 현재 위치 정보가 호환되지 않는다면, access가 거부된다.
- 접근 가능한 위치라면 위치정보와 그에 해당하는 구글맵 링크가 stdout으로 출력된다.

> 파일 생성시 accessibility를 테스트 하기 위해서는 `$ touch /proj4/파일명`을 하면 된다.
> - 만약 접근 가능 위치라면 `gpsupdate`로 세팅된 현재 위치 정보와 함께 파일을 정상적으로 생성, 수정한다.
> - 만약 접근 불가 위치라면 permission denied가 출력된다.

# Demo

## 1. 파일명: `snucse` 
### 파일 접근 불가 위치인 경우
- 파일 inode의 gps_location
    + latitude: 37.448862
    + longitude: 126.952387
    + accuracy: 50
- 현재 위치(latest_location)
    + latitude: 48.853910
    + longitude: 2.291351
    + accuracy: 50

![스크린샷 2019-12-17 오전 5 19 48](https://user-images.githubusercontent.com/17061663/70940142-0d124480-208d-11ea-8db4-6dd1f7f8bbe0.png)

### 파일 접근 가능 위치인 경우
- 파일 inode의 gps_location
    + latitude: 37.448862
    + longitude: 126.952387
    + accuracy: 50
- 현재 위치(latest_location)
    + latitude: 37.448701
    + longitude: 126.952698
    + accuracy: 50

![스크린샷 2019-12-17 오전 5 20 25](https://user-images.githubusercontent.com/17061663/70940148-0edc0800-208d-11ea-97bd-f05a6160c1a4.png)

- 아래는 출력된 링크를 웹 브라우저에 입력한 결과

![스크린샷 2019-12-17 오전 5 21 08](https://user-images.githubusercontent.com/17061663/70940152-10a5cb80-208d-11ea-9e85-6838c90ff398.png)

## 2. 파일명: `eiffel_tower`
- 파일 inode의 gps_location
    + latitude: 48.853910
    + longitude: 2.291351
    + accuracy: 50
    
<img width="464" alt="스크린샷 2019-12-17 오전 11 43 56" src="https://user-images.githubusercontent.com/17061663/70960503-95f7a300-20c2-11ea-9d9b-5f62e0e26dd2.png">

> 테스트 결과와 관련한 구체적인 내용은 데모 영상 참고

# Lessons
- 이번 과제에서는 file system의 구체적인 구조를 살펴보고, 파일 생성, 접근시 inode가 어떻게 활용되는지 확실하게 알 수 있었다.
- 프로젝트 진행 초반의 경우엔 `fs/ext2` 폴더 내의 소스들에서 파일 R/W 시에 어떤 동작이 발생하는지 이해하는 것이 가장 어려웠다.
- 파일 시스템을 이해한 뒤엔 `gps.c`의 여러가지 연산 함수들을 구현했는데 float 연산을 구조체를 통해 integer만으로 구현하는 것이 까다로웠다.
- 특히 두 location 사이의 위치를 계산하는 공식들을 찾아보고, 가장 효율적인 arc cosine 근사 공식을 찾아보고 실험하는 것이 힘들었다.
- 그 밖에 프로젝트 진행 중에 들었던 재밌는(?) 아이디어가 있다. 이제 새롭게 생성, 수정되는 파일마다 기존에 없던 metadata를 추가하는 방법을 알았으므로 gps_location 외에 재밌는 metadata를 추가할 수도 있다. 예를 들어서, text file의 경우엔 파일이 생성, 수정될 때마다 file content에 대해서 deep learning을 활용한 sentiment analysis를 수행하고, 그를 통해서 추출된 감정에 대한 태그를 metadata로 넣어주면 상당히 재밌을 것 같다.
