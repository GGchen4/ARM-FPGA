import torch  
import pandas as pd
import numpy as np
pthfile = r'./checkpoint_0040.pth'            #.pth文件的路径
model = torch.load(pthfile, torch.device('cpu'))    #设置在cpu环境下查询

for key,value in model["model"].items():
    print(key,value.size(),sep="   ")
print('type:')
print(type(model))  #查看模型字典长度
print('length:')
print(len(model))
print('key:')
for k in model.keys():
    print(k)

# stage1.block1.bn1.weight   torch.Size([16])
# 指定想要查看的参数名称
param_name = 'bn.running_var'  # 例如: 'conv.weight''stage1.block1.bn1.weight' 

# 检查参数是否存在于state_dict中
if param_name in model['model']:
    # 打印出指定参数的具体数值
    output = model['model'][param_name]
    # print(model['model'][param_name])
    array = output.numpy()
    print(array)

else:
    print(f"参数 {param_name} 不存在于模型的状态字典中。")

# with open('conv_weight.txt', 'a') as f:
#     f.write(param_name +"\n")  
#     f.write(np.array2string(array, threshold=np.inf))  
#     f.write('\n') 


# with open('stage1_block1_bn1_weight.txt', 'a') as f:
#     f.write(param_name + "\n")
    
#     # 将数组展开并格式化为用逗号分隔的字符串
#     array_flat = array.flatten()  # 如果是多维数组，将其展开为一维
#     # print(array_flat)
#     array_string = ','.join(map(str, array_flat))  # 将每个元素转换为字符串并用逗号连接
    
#     f.write(array_string)  # 写入数据
#     f.write('\n')  # 添加换行

# #字典中的key值即为csv中列名
# dataframe = pd.DataFrame(array)
 
# #将DataFrame存储为csv,index表示是否显示行名，default=True
# dataframe.to_csv("mpiigaze_resnet_preact.csv",index=False,sep=',')


# 检查是否只有一个键，且该键为'model'
# if len(model) == 1 and 'model' in model:
#     # 打印'model'键对应的每个参数的具体数值
#     for param_tensor in model['model']:
#         print(param_tensor, model['model'][param_tensor])

# print('value:')
# for k in model:         #查看模型字典里面的value
#     print(k,model[k])




