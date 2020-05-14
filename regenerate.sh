rm *.spvasm
spirv-dis conv-two-entry-points.spv > dis2.spvasm
sed -e '/^\%conv2d_1451x2311_same_ex_dispatch_0_dispatch_0 = OpFunction .*$/,$ d' dis2.spvasm > dis1.spvasm
sed -i -e '/conv2d_1451x2311_same_ex_dispatch_0_dispatch_0/ d' dis1.spvasm
spirv-as --target-env vulkan1.0 -o conv-one-entry-point.spv dis1.spvasm
