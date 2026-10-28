############################################################
## This file is generated automatically by Vivado HLS.
## Please DO NOT edit it.
## Copyright (C) 1986-2017 Xilinx, Inc. All Rights Reserved.
############################################################
open_project conv
set_top conv9x15x64_9x15x64
add_files conv_core.h
add_files conv_core.cpp
add_files -tb main.cpp
open_solution "32X18X30_TO_64X9X15"
set_part {xc7z035ffg676-2}
create_clock -period 10 -name default
#source "./conv/32X18X30_TO_64X9X15/directives.tcl"
csim_design
csynth_design
cosim_design
export_design -format ip_catalog
