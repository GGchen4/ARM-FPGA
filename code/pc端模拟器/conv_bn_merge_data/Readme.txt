对于stage1的第一个残差连接，也即stage1_shortcut，在经过卷积——BN融合之后，是没有偏置bias的，而且步长应为1，且不填充

Conv2d(16,36,60,16,1,1,1,1,0,0,conv164,15,stage1_int16_short_conv_buffer_addr64,15,stage1_block1_shortcut64,15);

对于stage2和stage3的残差连接，也即stage1_shortcut，在经过卷积——BN融合之后，也是没有偏置bias的，但步长应为2，且不填充

以stage2为例：

Conv2d(16,36,60,32,1,1,2,2,0,0,stage1_block1_shortcut64,15,stage2_int16_short_conv_buffer_addr64,15,stage2_block1_shortcut64,15);