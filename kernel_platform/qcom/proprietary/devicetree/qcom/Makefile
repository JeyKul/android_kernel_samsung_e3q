# SPDX-License-Identifier: GPL-2.0
# COPYRIGHT(C) 2023 Samsung Electronics Co., Ltd. All Right Reserved.

dtstree		:= $(srctree)/$(src)
e3q_boards	+= $(patsubst $(dtstree)/%.dts,%.dtb, $(wildcard $(dtstree)/*.dts))
e3q_boards	+= $(patsubst $(dtstree)/%.dts,%.dtbo, $(wildcard $(dtstree)/*.dts))

dtb-$(CONFIG_ARCH_PINEAPPLE)	+= $(e3q_boards)
