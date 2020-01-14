%ifarch aarch64
%define config_name tizen_bcmrpi3_defconfig
%define buildarch arm64
%else
%define config_name tizen_bcm2709_defconfig
%define buildarch arm
%endif
%define target_board rpi3
%define variant %{buildarch}-%{target_board}

Name: %{target_board}-linux-kernel
Summary: The Linux Kernel for Raspberry Pi3
Version: 4.14.67
Release: 0
License: GPL-2.0
ExclusiveArch: %{arm} aarch64
Group: System/Kernel
Vendor: The Linux Community
URL: https://www.kernel.org
Source0:   linux-kernel-%{version}.tar.xz
BuildRoot: %{_tmppath}/%{name}-%{PACKAGE_VERSION}-root

%define fullVersion %{version}-%{variant}

BuildRequires: bc
BuildRequires: module-init-tools
BuildRequires: u-boot-tools >= 2016.03

%description
The Linux Kernel, the operating system core itself

%package -n %{variant}-linux-kernel
License: GPL-2.0
Summary: Tizen kernel for %{target_board}
Group: System/Kernel
Provides: %{variant}-kernel-uname-r = %{fullVersion}
Provides: linux-kernel = %{version}-%{release}

%description -n %{variant}-linux-kernel
This package contains the Linux kernel for Tizen (arch %{buildarch}, target board %{target_board})

%package -n %{variant}-linux-kernel-modules
Summary: Kernel modules for %{target_board}
Group: System/Kernel
Provides: %{variant}-kernel-modules = %{fullVersion}
Provides: %{variant}-kernel-modules-uname-r = %{fullVersion}

%description -n %{variant}-linux-kernel-modules
Kernel-modules includes the loadable kernel modules(.ko files) for %{target_board}

%package -n %{variant}-linux-kernel-devel
License: GPL-2.0
Summary: Linux support kernel map and etc for other packages
Group: System/Kernel
Provides: %{variant}-kernel-devel = %{fullVersion}
Provides: %{variant}-kernel-devel-uname-r = %{fullVersion}

%description -n %{variant}-linux-kernel-devel
This package provides kernel map and etc information.

%prep
%setup -q -n linux-kernel-%{version}

%build
%{?asan:/usr/bin/gcc-unforce-options}
%{?ubsan:/usr/bin/gcc-unforce-options}

# Make sure EXTRAVERSION says what we want it to say
sed -i "s/^EXTRAVERSION.*/EXTRAVERSION = -%{variant}/" Makefile

# 1-1. Set config file
make %{config_name}

# 1-2. Build Image/Image.gz
make %{?_smp_mflags}

# 1-3. Build dtbs
make dtbs %{?_smp_mflags}

# 1-4. Build modules
make modules %{?_smp_mflags}

%install
QA_SKIP_BUILD_ROOT="DO_NOT_WANT"; export QA_SKIP_BUILD_ROOT

# 2-1. Destination directories
mkdir -p %{buildroot}/boot
mkdir -p %{buildroot}/lib/modules

# 2-2. Install kernel binary and DTB
%ifarch aarch64
install -m 644 arch/%{buildarch}/boot/Image %{buildroot}/boot/
install -m 644 arch/%{buildarch}/boot/dts/broadcom/bcm*.dtb %{buildroot}/boot/
%else
install -m 644 arch/%{buildarch}/boot/zImage %{buildroot}/boot/
install -m 644 arch/%{buildarch}/boot/dts/bcm*.dtb %{buildroot}/boot/
%endif

# 2-3. Install modules
make INSTALL_MOD_STRIP=1 INSTALL_MOD_PATH=%{buildroot} modules_install

# 2-4. Install boot binary files
%ifarch aarch64
install -m 644 rpi3/boot/config_64bit.txt %{buildroot}/boot/config.txt
%else
install -m 644 rpi3/boot/config_32bit.txt %{buildroot}/boot/config.txt
%endif
install -m 644 rpi3/boot/LICENCE.broadcom %{buildroot}/boot/
install -m 644 rpi3/boot/bootcode.bin %{buildroot}/boot/
install -m 644 rpi3/boot/start*.elf %{buildroot}/boot/
install -m 644 rpi3/boot/fixup*.dat %{buildroot}/boot/

# 3-1. remove unnecessary files to prepare for devel package
find %{_builddir}/linux-kernel-%{version} -name ".tmp_vmlinux*" -delete
find %{_builddir}/linux-kernel-%{version} -name ".gitignore" -delete
find %{_builddir}/linux-kernel-%{version} -name "\.*dtb*tmp" -delete
find %{_builddir}/linux-kernel-%{version} -name "\.*dtb" -delete
find %{_builddir}/linux-kernel-%{version} -name "*\.*tmp" -delete
find %{_builddir}/linux-kernel-%{version} -name "vmlinux" -delete
find %{_builddir}/linux-kernel-%{version} -name "Image" -delete
find %{_builddir}/linux-kernel-%{version} -name "zImage" -delete
find %{_builddir}/linux-kernel-%{version} -name "Image.gz" -delete
find %{_builddir}/linux-kernel-%{version} -name "*.cmd" -delete
find %{_builddir}/linux-kernel-%{version} -name "*\.ko" -delete
find %{_builddir}/linux-kernel-%{version} -name "*\.o" -delete
find %{_builddir}/linux-kernel-%{version} -name "*\.S" -delete
find %{_builddir}/linux-kernel-%{version} -name "*\.HEX" -type f -delete
find %{_builddir}/linux-kernel-%{version} -name "*\.c" -not -path "%{_builddir}/linux-kernel-%{version}/scripts/*" -delete

# 3-2. move files for devel package
cp -r  %{_builddir}/linux-kernel-%{version}/ %{_builddir}/kernel-devel-%{variant}/

# 4. Move files for each package
mkdir -p %{buildroot}/boot/kernel/devel
mv %{_builddir}/kernel-devel-%{variant} %{buildroot}/boot/kernel/devel/


%clean
rm -rf %{buildroot}

%files -n %{variant}-linux-kernel-modules
/lib/modules/*

%files -n %{variant}-linux-kernel-devel
/boot/kernel/devel/*

%files -n %{variant}-linux-kernel
%license COPYING
%ifarch aarch64
/boot/Image
%else
/boot/zImage
%endif
/boot/bcm*.dtb
/boot/config.txt
/boot/LICENCE.broadcom
/boot/bootcode.bin
/boot/start*.elf
/boot/fixup*.dat
