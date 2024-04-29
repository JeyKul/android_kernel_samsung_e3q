# SPDX-License-Identifier: GPL-2.0
# COPYRIGHT(C) 2023 Samsung Electronics Co., Ltd. All Right Reserved.

__usb_platform_map =  {
    "pineapple": {
        "gki": [
            # keep sorted & in-tree modules only
            "drivers/usb/gadget/function/usb_f_conn_gadget.ko",
            "drivers/usb/gadget/function/usb_f_ss_acm.ko",
            "drivers/usb/gadget/function/usb_f_ss_mon_gadget.ko",
            "drivers/usb/misc/ehset.ko",
			"drivers/usb/misc/lvstest.ko",
            "drivers/usb/repeater/repeater-i2c-eusb2.ko",
			"drivers/net/usb/smsc75xx.ko",
			"drivers/net/usb/smsc95xx.ko",
			"drivers/net/phy/smsc.ko",
        ],
        "consolidate": [
            # keep sorted
        ],
    },
}

def sec_usb(target, variant):
    if not target in __usb_platform_map:
        return []

    target_map = __usb_platform_map[target]
    if not variant in target_map:
        return []

    if variant == "consolidate":
        return target_map[variant] + target_map["gki"]

    return target_map[variant]
