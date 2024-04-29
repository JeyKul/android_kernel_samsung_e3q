load("//build/bazel_common_rules/dist:dist.bzl", "copy_to_dist_dir")
load("//build/kernel/kleaf:kernel.bzl", "ddk_module")

def define_aps(target, variant):
    kernel_build_variant = "{}_{}".format(target, variant)
    include_base = "../../../{}".format(native.package_name())

    ddk_module(
        name = "{}_aps".format(kernel_build_variant),
        out = "rmnet_aps.ko",
        srcs = [
            "rmnet_aps_genl.c",
            "rmnet_aps_main.c",
            "rmnet_aps.h",
            "rmnet_aps_genl.h",
        ],
        kernel_build = "//msm-kernel:{}".format(kernel_build_variant),
        deps = [
            "//msm-kernel:all_headers",
            "//vendor/qcom/opensource/datarmnet:{}_rmnet_core".format(kernel_build_variant),
            "//vendor/qcom/opensource/datarmnet:rmnet_core_headers",
        ],
        copts = ["-Wno-misleading-indentation"],
        includes = ["include"],
    )

    copy_to_dist_dir(
        name = "{}_datarment-ext_dist".format(kernel_build_variant),
        data = [
            ":{}_aps".format(kernel_build_variant),
        ],
        dist_dir = "out/target/product/{}/dlkm/lib/modules/".format(target),
        flat = True,
        wipe_dist_dir = False,
        allow_duplicate_filenames = False,
        mode_overrides = {"**/*": "644"},
    )
