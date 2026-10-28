############################################################
## This file is generated automatically by Vivado HLS.
## Please DO NOT edit it.
## Copyright (C) 1986-2017 Xilinx, Inc. All Rights Reserved.
############################################################
open_project conv_acc
set_top Conv
add_files conv_core.cpp
add_files conv_core.h
add_files -tb main.cpp
open_solution "solution1"
set_part {xc7z035ffg676-2} -tool vivado
create_clock -period 10 -name default
#source "./conv_acc/solution1/directives.tcl"
csim_design
csynth_design
cosim_design
export_design -format ip_catalog
