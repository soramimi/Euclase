nvcc -c -O2 libeuclasecuda.cu 
link /DLL libeuclasecuda.obj "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.1\lib\x64\cudart.lib" /DEF:libEuclaseCUDA.def /OUT:libEuclaseCUDA.dll
