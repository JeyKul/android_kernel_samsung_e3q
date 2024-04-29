load("//build/kernel/kleaf:kernel.bzl", "ddk_module","ddk_submodule")
load("//build/bazel_common_rules/dist:dist.bzl", "copy_to_dist_dir")

def _register_module_to_map(module_map, name, path, config_option, srcs):
    module = struct(
        name = name,
        path = path,
        srcs = srcs,
        config_option = config_option
    )

    module_map[name] = module

def _get_config_choices(map, options):
    choices = []
    for option in map:
        choices.extend(map[option].get(option in options,[]))
    return choices

def _get_kernel_build_options(modules, config_options):
    all_options = {option: True for option in config_options}
    all_options = all_options | {module.config_option: True for module in modules if module.config_option}
    return all_options

def _get_kernel_build_module_srcs(module, options, formatter):
    srcs = module.srcs
    print("-",module.name,",",module.config_option,",srcs =",srcs)
    module_path = "{}/".format(module.path) if module.path else ""
    return ["{}{}".format(module_path, formatter(src)) for src in srcs]

def touch_module_entry(hdrs = []):
    module_map = {}

    def register(name, path = None, config_option = [], srcs = [], config_srcs = None, deps = None):
        _register_module_to_map(module_map, name, path, config_option, srcs)
    return struct(
        register = register,
        get = module_map.get,
        hdrs = hdrs
    )

def define_target_variant_modules(target, variant, registry, modules, config_options = []):
    kernel_build = "{}_{}".format(target, variant)
    kernel_build_label = "//msm-kernel:{}".format(kernel_build)
    modules = [registry.get(module_name) for module_name in modules]
    options = _get_kernel_build_options(modules, config_options)
    build_print = lambda message : print("{}: {}".format(kernel_build, message))
    formatter = lambda s : s.replace("%b", kernel_build).replace("%t", target)
    headers = ["//msm-kernel:all_headers"] + registry.hdrs
    all_module_rules = []

    for module in modules:
        rule_name = "{}_{}".format(kernel_build, module.name)
        module_srcs = _get_kernel_build_module_srcs(module, options, formatter)

        if not module_srcs:
            continue

        ddk_submodule(
            name = rule_name,
            srcs = module_srcs,
            out = "{}.ko".format(module.name),
            deps =  headers,
            local_defines = options.keys(),
        )
        all_module_rules.append(rule_name)

    ddk_module(
        name = "{}_touch_drivers".format(kernel_build),
        kernel_build = kernel_build_label,
        deps = all_module_rules,
    )
    copy_to_dist_dir(
        name = "{}_touch_drivers_dist".format(kernel_build),
        data = [":{}_touch_drivers".format(kernel_build)],
        dist_dir = "out/target/product/{}/dlkm/lib/modules".format(target),
        flat = True,
        wipe_dist_dir = False,
        allow_duplicate_filenames = False,
        mode_overrides = {"**/*": "644"},
        log = "info",
    )

def define_consolidate_gki_modules(target, registry, modules, config_options = []):
        define_target_variant_modules(target, "gki", registry, modules, config_options)
        define_target_variant_modules(target, "consolidate", registry, modules, config_options)
