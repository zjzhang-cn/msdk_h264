# msdk_h264 for pion mediadevices

## 根据 [Intel Media SDK h.264 encoder sample](https://github.com/Intel-Media-SDK/MediaSDK/blob/master/tutorials/simple_3_encode/src/simple_encode.cpp) 去实现 [Pion Mediadevices](https://github.com/pion/mediadevices) 的H246编码.

## 安装依赖
根据[Intel® software for general purpose GPU capabilities](https://dgpu-docs.intel.com)安装Intel Media SDK

```sh
sudo apt-get install -y gpg-agent wget
wget -qO - https://repositories.intel.com/graphics/intel-graphics.key |
  sudo apt-key add -
sudo apt-add-repository \
  'deb [arch=amd64] https://repositories.intel.com/graphics/ubuntu focal main'
sudo apt-get update
sudo apt-get install \
  intel-opencl-icd \
  intel-level-zero-gpu level-zero \
  intel-media-va-driver-non-free libmfx1 libva-drm2
sudo apt install libmfx-dev
```

