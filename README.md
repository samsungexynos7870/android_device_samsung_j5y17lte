# TWRP for the Samsung Galaxy J5 2017

TWRP device tree for the Samsung Galaxy J5 2017 (SM-J530F/FD/G, `j5y17lte`),
based on the TWRP AOSP 12.1 manifest.

### How to build ###

```bash
# Create dirs
$ mkdir twrp ; cd twrp

# Init repo
$ repo init --depth=1 -u https://github.com/minimal-manifest-twrp/platform_manifest_twrp_aosp.git -b twrp-12.1

# Add the device tree and its kernel
$ mkdir -p .repo/local_manifests
$ cat > .repo/local_manifests/j5y17lte.xml <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<manifest>
    <remote fetch="https://github.com/samsungexynos7870" name="exynos7870-labs" />

    <project name="android_device_samsung_j5y17lte" path="device/samsung/j5y17lte"
             remote="exynos7870-labs" revision="twrp-12.1" />
    <project name="android_kernel_samsung_exynos7870" path="kernel/samsung/exynos7870"
             remote="exynos7870-labs" revision="twrp-12.1" />
</manifest>
EOF

# Sync
$ repo sync

# Build
$ source build/envsetup.sh ; lunch twrp_j5y17lte-eng ; mka recoveryimage
```

The recovery image is `out/target/product/j5y17lte/recovery.img`.

## Credits
2020 @Astrako
2026 samsungexynos7870

`dtbhtool/` is the DTBH tooling of the LineageOS project
(`LineageOS/android_hardware_samsung`, branch `lineage-19.1`), vendored
unchanged.

## Contact
Telegram support group: https://t.me/joinchat/D1Jk_VbieGBXOWZt2y8O7A
