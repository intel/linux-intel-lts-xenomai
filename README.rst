Guide for build and installation
###############################################################################

.. code-block:: console

  $git clone https://github.com/intel/linux-intel-lts-xenomai
  $git checkout F/4.19.59/base/ipipe/xenomai_3.1-rc3

  $mkdir ../out
  $cp arch/x86/configs/xenomai_test_defconfig ../out/.config
  $make O=../out oldconfig
  $make bindeb-pkg O=../out KDEB_PKGVERSION=3.1-rc3 -j$(nproc --all)

if build successfully, deb packages will appear outside kernel tree:

.. code-block:: console

  linux-headers-4.19.59-xenomai+_3.1-rc3_amd64.deb
  linux-image-4.19.59-xenomai+_3.1-rc3_amd64.deb
  linux-libc-dev_3.1-rc3_amd64.deb

then you can install them on Ubuntu or Debian by:

.. code-block:: console

  $sudo dpkg -i linux-image-4.19.59-xenomai+_3.1-rc3_amd64.deb
  $sudo dpkg -i linux-headers-4.19.59-xenomai+_3.1-rc3_amd64.deb
  $sudo dpkg -i linux-libc-dev_3.1-rc3_amd64.deb
