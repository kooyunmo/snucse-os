#ifndef SYS_SET_GPS_LOCATION
#define SYS_SET_GPS_LOCATION

#include <linux/gps.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/namei.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/math64.h>

#define FRAC_MAX    1000000L
#define FRAC_MIN    0


struct gps_location latest_location = {
    .lat_integer = 0,
    .lat_fractional = 0,
    .lng_integer = 0,
    .lng_fractional = 0,
    .accuracy = 0,
};

DEFINE_MUTEX(l_gps);

typedef struct gps_fixed_point {
    int integer;
    int fraction;
} gps_fixp;

void gps_fixp_init(int integer, int fraction, gps_fixp *ret) {
    ret->integer = integer;
    ret->fraction = fraction;
}


void gps_fixp_add(gps_fixp *a, gps_fixp *b, gps_fixp * ret) {
	gps_fixp temp_a;
	gps_fixp temp_b;
	gps_fixp_init(a->integer, a->fraction, &temp_a);
	gps_fixp_init(b->integer, b->fraction, &temp_b);
    ret->integer = temp_a.integer + temp_b.integer;
    ret->integer = ret->integer + (temp_a.fraction + temp_b.fraction) / FRAC_MAX;
	ret->fraction = (temp_a.fraction + temp_b.fraction) % FRAC_MAX;
}

void gps_fixp_sub(gps_fixp *a, gps_fixp *b, gps_fixp *ret) {
	gps_fixp temp_a;
	gps_fixp temp_b;
	gps_fixp_init(a->integer, a->fraction, &temp_a);
	gps_fixp_init(b->integer, b->fraction, &temp_b);
    if (temp_a.fraction < temp_b.fraction) {
        temp_a.integer -= 1;
        temp_a.fraction += FRAC_MAX;
    }
    ret->fraction = temp_a.fraction - temp_b.fraction;
    ret->integer = temp_a.integer - temp_b.integer;
}

void gps_fixp_mul(gps_fixp *a, gps_fixp *b, gps_fixp *ret) {
    // integer part
    s64 ai_bi = (s64)a->integer * (s64)b->integer;
    
    // fraction part
    s32 remainder;
    s64 ai_bf = (s64)a->integer * (s64)b->fraction;
    s64 af_bi = (s64)a->fraction * (s64)b->integer;
    s64 af_bf = (s64)a->fraction * (s64)b->fraction;
    s64 temp = ai_bf + af_bi + div64_s64(af_bf, FRAC_MAX);
    ret->integer = (s32)ai_bi;
    ret->integer += (s32)div_s64_rem(temp, (s32)FRAC_MAX, &remainder);
    
    if(remainder < FRAC_MIN) {
        ret->integer -= 1;
        remainder = (s32)FRAC_MAX + remainder;
    }
    ret->fraction = remainder;
}

void gps_fixp_div_int(gps_fixp * a, int n, gps_fixp * ret){
	ret->fraction=(((a->integer % n)*1000000+a->fraction)/n)%1000000;
	ret->integer=a->integer/n;
}///////fix

int gps_fixp_factorial(int n){

	int ret=1;
	int i;
	for(i=2;i<=n;i++){
		ret=ret*i;
	}
	return ret;
}

void gps_fixp_pow(gps_fixp * a, int n, gps_fixp * ret){
	int i;
	gps_fixp_init(a->integer, a->fraction, ret);
	for(i=0;i<n-1;i++){
		gps_fixp_mul(a, ret, ret);
	}
}

void gps_fixp_deg2rad(gps_fixp *deg, gps_fixp *rad){
	gps_fixp temp;
	gps_fixp_init(0, 17453, &temp);// pi/180
	gps_fixp_mul(deg, &temp, rad);
}

void gps_fixp_cos(gps_fixp * a, gps_fixp *ret){
    gps_fixp a2rad;
    gps_fixp pi;
    gps_fixp minus_1;
    gps_fixp_init(3, 141593, &pi);
    gps_fixp_init(-1, 0, &minus_1);
    
    gps_fixp_deg2rad(a, &a2rad);
    if(a->integer > 90) {
        gps_fixp_sub(&pi, &a2rad, &a2rad);
    }
	gps_fixp_init(1, 0, ret);
	gps_fixp temp;
	int i;
	for(i=1;i<7;i++){
	    gps_fixp_pow(&a2rad, i*2, &temp);
	    gps_fixp_div_int(&temp, gps_fixp_factorial(i*2), &temp);
	    if(i%2)
		    gps_fixp_sub(ret, &temp, ret);
	    else
		    gps_fixp_add(ret, &temp, ret);
	}
	
	if(a->integer > 90) {
	    gps_fixp_mul(ret, &minus_1, ret);
	}
}

void gps_fixp_sin(gps_fixp * a, gps_fixp *ret){
    gps_fixp a2rad;
    gps_fixp pi;
    gps_fixp_init(3, 141593, &pi);
    
    gps_fixp_deg2rad(a, &a2rad);
    if(a->integer > 90) {
        gps_fixp_sub(&pi, &a2rad, &a2rad);
    }
	gps_fixp_init(a2rad.integer, a2rad.fraction, ret);
	gps_fixp temp;
	int i;
	for(i=1;i<7;i++){
	    gps_fixp_pow(&a2rad, i*2+1, &temp);
	    gps_fixp_div_int(&temp, gps_fixp_factorial(i*2+1), &temp);
	    if(i%2)
		    gps_fixp_sub(ret, &temp, ret);
	    else
		    gps_fixp_add(ret, &temp, ret);
	    //printk(KERN_ALERT "sin at iter %d: %d.%06d", i, ret->integer, ret->fraction);
	}
}

void gps_fixp_acos(gps_fixp *a, gps_fixp *ret){
	//pi/2-x-(x^3/6)-(3*x^5/40)-(5*x^7/112)-(35*x^9/1152)-(63/2816*x^11)
	gps_fixp temp;
	gps_fixp temp2;
	
	gps_fixp_init(1, 570796, ret);//pi/2
	gps_fixp_sub(ret, a, ret);
	
	gps_fixp_pow(a, 3, &temp);
	gps_fixp_init(0,166667,&temp2);// 1/6
	gps_fixp_mul(&temp, &temp2, &temp);
	gps_fixp_sub(ret, &temp, ret);
	//printk(KERN_ALERT "acos at iter 1: %d.%06d",  ret->integer, ret->fraction);
	
	gps_fixp_pow(a, 5, &temp);
	gps_fixp_init(0,75000, &temp2);// 3/40
	gps_fixp_mul(&temp, &temp2, &temp);
	gps_fixp_sub(ret, &temp, ret);
	//printk(KERN_ALERT "acos at iter 2: %d.%06d",  ret->integer, ret->fraction);
	
	gps_fixp_pow(a, 7, &temp);
	gps_fixp_init(0,44643, &temp2);// 5/112
	gps_fixp_mul(&temp, &temp2, &temp);
	gps_fixp_sub(ret, &temp, ret);
	//printk(KERN_ALERT "acos at iter 3: %d.%06d",  ret->integer, ret->fraction);

	gps_fixp_pow(a, 9, &temp);
	gps_fixp_init(0, 30382, &temp2);// 35/1152
	gps_fixp_mul(&temp, &temp2, &temp);
	gps_fixp_sub(ret, &temp, ret);
	//printk(KERN_ALERT "acos at iter 4: %d.%06d",  ret->integer, ret->fraction);

	gps_fixp_pow(a, 11, &temp);
	gps_fixp_init(0, 22372, &temp2);// 63/2816
	gps_fixp_mul(&temp, &temp2, &temp);
	gps_fixp_sub(ret, &temp, ret);
	//printk(KERN_ALERT "acos at iter 5: %d.%06d",  ret->integer, ret->fraction);
	if(ret->integer ==0){
		if(ret->fraction<260000){
			gps_fixp_init(0, 300000, &temp2);
			gps_fixp_sub(ret, &temp2, ret);
		}
		else if(ret->fraction < 310000){
			gps_fixp_init(0, 200000, &temp2);
			gps_fixp_sub(ret, &temp2, ret);	
		}
		else if(ret->fraction < 360000){
			gps_fixp_init(0, 60000, &temp2);
			gps_fixp_sub(ret, &temp2, ret);
		}
		else if(ret->fraction < 440000){	
			gps_fixp_init(0, 40000, &temp2);
			gps_fixp_sub(ret, &temp2, ret);
		}
	
	}
	else if (ret->integer ==2){
		if(ret->fraction>800000){
			gps_fixp_init(0, 50000, &temp2);
			gps_fixp_add(ret, &temp2, ret);
		}
		else if(ret->fraction > 860000){
			gps_fixp_init(0, 120000, &temp2);
			gps_fixp_add(ret, &temp2, ret);
		}
		else if(ret->fraction > 880000){
			gps_fixp_init(0, 200000, &temp2);
			gps_fixp_add(ret, &temp2, ret);
		}
	}
}





int cmp_file_loc_and_latest_loc(struct gps_location *loc) {
    gps_fixp file_lat, file_lng;        // beta
    gps_fixp latest_lat, latest_lng;  // gamma
    
    gps_fixp_init(loc->lat_integer, loc->lat_fractional, &file_lat);
    gps_fixp_init(loc->lng_integer, loc->lng_fractional, &file_lng);
    gps_fixp_init(latest_location.lat_integer, latest_location.lat_fractional, &latest_lat);
    gps_fixp_init(latest_location.lng_integer, latest_location.lng_fractional, &latest_lng);
    
    gps_fixp _90d;
    gps_fixp alpha, beta, gamma, largeA;
    gps_fixp_init(90, 0, &_90d);                // 90 degree
    gps_fixp_sub(&_90d, &file_lat, &beta);      // beta
    gps_fixp_sub(&_90d, &latest_lat, &gamma);   // gamma
    
    // largeA : difference of latitude between two locations.
    // (this should be absolute value.)
    if(file_lng.integer > latest_lng.integer) {
        gps_fixp_sub(&file_lng, &latest_lng, &largeA);
    } else if(file_lng.integer == latest_lng.integer) {
        if(file_lng.fraction > latest_lng.fraction) {
            gps_fixp_sub(&file_lng, &latest_lng, &largeA);
        } else {
            gps_fixp_sub(&latest_lng, &file_lng, &largeA);
        }
    } else {
        gps_fixp_sub(&latest_lng, &file_lng, &largeA);
    }
    
    gps_fixp cosa, cosb, sinb, cosg, sing, cosA;
    gps_fixp_cos(&beta, &cosb);     // cos(beta)
    gps_fixp_sin(&beta, &sinb);     // sin(beta)
    gps_fixp_cos(&gamma, &cosg);    // cos(gamma)
    gps_fixp_sin(&gamma, &sing);    // sin(gamma)
    gps_fixp_cos(&largeA, &cosA);   // cos(A)
    
    gps_fixp cosb_mul_cosg;         // cos(beta) * cos(gamma)
    gps_fixp_mul(&cosb, &cosg, &cosb_mul_cosg);
    gps_fixp sinb_mul_sing;         // sin(beta) * sin(gamma)
    gps_fixp_mul(&sinb, &sing, &sinb_mul_sing);
    
    gps_fixp sinb_mul_sing_mul_cosA;    // sin(beta) * sin(gamma) * cos(A)
    gps_fixp_mul(&sinb_mul_sing, &cosA, &sinb_mul_sing_mul_cosA);
    
    // cos(alpha) = minimum degree between two locations
    // cos(alpha) = cos(beta) * cos(gamma) + sin(beta) * sin(gamma) * cos(A)
    gps_fixp_add(&cosb_mul_cosg, &sinb_mul_sing_mul_cosA, &cosa);
    
    gps_fixp_acos(&cosa, &alpha);       // alpha
    
    gps_fixp radius;            // earth radius
    gps_fixp distance;          // distance between two locations (km)
    gps_fixp_init(6371, 0, &radius);        // radius
    
    gps_fixp_mul(&alpha, &radius, &distance);   // distance
    
    gps_fixp latest_location_accuracy;
    gps_fixp file_loc_accuracy;
    gps_fixp_init(latest_location.accuracy/1000, ((latest_location.accuracy)%1000)*1000, &latest_location_accuracy);
    gps_fixp_init(loc->accuracy/1000, ((loc->accuracy)%1000)*1000, &file_loc_accuracy);
    
    gps_fixp sum_accuracy;
    gps_fixp_add(&latest_location_accuracy, &file_loc_accuracy, &sum_accuracy);
    
    gps_fixp ok_fail;
    gps_fixp_sub(&sum_accuracy, &distance, &ok_fail);
    
    if (ok_fail.integer < 0) {
        // fail
		printk(KERN_ALERT "distance too far!: %d.%06d", distance.integer, distance.fraction);
		//printk(KERN_ALERT "sum_accuracy: %d.%06d", sum_accuracy.integer, sum_accuracy.fraction);
		//printk(KERN_ALERT "ok_fail: %d.%06d", ok_fail.integer, ok_fail.fraction);

        return -1;
    } else {
        return 0;
    }
}

static int is_valid(struct gps_location *loc) {
    int lat_integer = loc->lat_integer;
    int lat_fractional = loc->lat_fractional;
    int lng_integer = loc->lng_integer;
    int lng_fractional = loc->lng_fractional;
    int accuracy = loc->accuracy;
    
    if (lat_fractional < FRAC_MIN || lat_fractional >= FRAC_MAX) {
        return 0;
    }
    if (lng_fractional < FRAC_MIN || lng_fractional >= FRAC_MAX) {
        return 0;
    }
    
    if ((lat_integer < -90) || (lat_integer > 90) ||
        (lat_integer == -90 && lat_fractional > 0) ||
        (lat_integer == 90 && lat_fractional > 0)) {
        printk(KERN_ALERT "RANGE ERROR: The latitude range is [-90:90]\n");  
        return 0; 
    }
    if ((lng_integer < -180) || (lng_integer > 180) ||
        (lng_integer == -180 && lng_fractional > 0) ||
        (lng_integer == 180 && lng_fractional > 0)) {
        printk(KERN_ALERT "RANGE ERROR: The longitude range is [-180:180]\n");
        return 0;
    }
    
    if(accuracy < 0) {
        printk(KERN_ALERT "The accuracy must be a positive integer\n");
        return 0;
    }
    // pass all validation test
    return 1; 
}


long sys_set_gps_location(struct gps_location __user *loc)
{
    printk(KERN_ALERT "sys_set_gps_location\n");
	if(loc==NULL){
		
		printk(KERN_ALERT "INVALID: NULL\n");
		return -EFAULT;
	}
	struct gps_location *temp=(struct gps_location*)kmalloc(sizeof(struct gps_location), GFP_KERNEL);
	if(copy_from_user(temp, loc, sizeof(struct gps_location))!=0){
	    printk(KERN_ALERT "FAILED: sys_gps_location: copy_from_user failed\n");
		kfree(temp);
		return -1;
	}
	
	if(!is_valid(temp)) {
	    kfree(temp);
	    return -1;
	}
	
	gps_fixp t;
	gps_fixp t2;
	gps_fixp t3;
	gps_fixp_init(temp->lat_integer, temp->lat_fractional, &t);
	
	gps_fixp_init(temp->lat_integer, temp->lat_fractional, &t2);
	gps_fixp_deg2rad(&t, &t);
	
	mutex_lock(&l_gps);
	latest_location.lat_integer=temp->lat_integer;
	latest_location.lat_fractional=temp->lat_fractional;
	latest_location.lng_integer=temp->lng_integer;
	latest_location.lng_fractional=temp->lng_fractional;
	latest_location.accuracy=temp->accuracy;
	mutex_unlock(&l_gps);
	//printk(KERN_ALERT "%d.%06d lat: updated",latest_location.lat_integer, latest_location.lat_fractional);
	//printk(KERN_ALERT "%d.%06d lng: updated",latest_location.lng_integer, latest_location.lng_fractional);
	//printk(KERN_ALERT "%d accuracy: updated",latest_location.accuracy);
	kfree(temp);
	return 0;
}

long sys_get_gps_location(const char __user *pathname, struct gps_location __user *loc)
{
	char * path;
	struct inode * cur;
	struct path cur_path;
	long path_length;
    printk(KERN_ALERT "sys_get_gps_location\n");
	if(pathname==NULL || loc==NULL){
		printk(KERN_ALERT "INVALID: NULL ptr\n");
		return -EINVAL;
	}
	if(!(path_length=strnlen_user(pathname, 100000L))){
		printk(KERN_ALERT "INVALID: path length 0\n");
		return -EINVAL;
	}
	path=(char *)kmalloc(sizeof(char)*path_length, GFP_KERNEL);
	if(strncpy_from_user(path, pathname, path_length)<0){
		kfree(path);
		printk(KERN_ALERT "INVALID: invalid path name\n");
		return -EINVAL;
	}
	if(kern_path(path, LOOKUP_FOLLOW, &cur_path)){
		printk(KERN_ALERT "INVALID: cannot get struct path\n");
		kfree(path);
		return -EINVAL;
	}
	cur=cur_path.dentry->d_inode;

    if (inode_permission(cur, MAY_READ)) {
		printk(KERN_ALERT "FAILED: no permission\n");
        return -EACCES;
    }
	struct gps_location *temp=(struct gps_location*)kmalloc(sizeof(struct gps_location), GFP_KERNEL);
	if(copy_from_user(temp, loc, sizeof(struct gps_location))!=0){
		printk(KERN_ALERT "FAILED: cannot access to user space\n");
		kfree(temp);
		return 0;
	}
		
    if (cur->i_op->get_gps_location) {
		if((cur->i_op->get_gps_location(cur, temp))!=0){
			printk(KERN_ALERT "FAILED: get_gps_location function not working\n");
			kfree(temp);
			return -EINVAL;
		}
    }
	else{
		printk(KERN_ALERT "FAILED: no gps system embedded\n");
        return -ENODEV;
	}

	if(copy_to_user(loc, temp, sizeof(struct gps_location))!=0){
		printk(KERN_ALERT "FAILED: cannot write to user space\n");
		kfree(temp);
		return -EINVAL;
	}
	printk(KERN_ALERT "SUCCESS\n");
	kfree(temp);
    return 0;
}

int nearby(struct inode * current_inode){
    // this is location where the file is created or modified most recently
    struct gps_location *file_location;
    file_location = (struct gps_location *)kmalloc(sizeof(struct gps_location), GFP_KERNEL);
    if (file_location == NULL) {
        printk(KERN_ALERT "FAILED: kmalloc failed in function: nearby\n");
        return -1;
    }
    if (current_inode->i_op->get_gps_location) {
        if (current_inode->i_op->get_gps_location(current_inode, file_location) != 0) {
            kfree(current_inode);
            printk(KERN_ALERT "FAILED: cannot assign file's gps location to current inode\n");
            return -1;
        }
        printk(KERN_ALERT "SUCCESS: file gps location is assigned to current inode\n");
    } else {
        kfree(current_inode);
        printk(KERN_ALERT "FAILED: cannot find i_op->get_gps_location\n");
        return -1;
    }
    
    // some compare function: compare current_inode and latest_location
    if(cmp_file_loc_and_latest_loc(file_location)) {
        printk("You are not in accessible location.");
        return -1;
    }
    
    printk(KERN_ALERT "SUCCESS: Currently in the nearby location. Access allowed!");
	return 0;
}

#endif /* SYS_SET_GPS_LOCATION */
