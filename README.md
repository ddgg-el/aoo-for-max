AOO for Max
===========

### Overview
This repository contains the source code for the `AOO for Max` package. There is no need to manually build your project. To download a ready to use package please visit the Release page or download it from the [Max Package Manager](https://cycling74.com/packages/aoo-for-max).

The externals has been tested on Max8.1 running on MacOS 12.1 and Windows11. They should also work with more modern releases.

Since version 0.9.1 the package includes Opus support for compressed audio streaming.

#### Folder structure
```
├── CMakeLists.txt
├── LICENSE
├── README.md
├── aoo
├── max-sdk-base
├── package
│   └── AOO for Max <---- the Max package
├── package-info.json
└── source
```
The `source` folder contains the source file for each external while the `package` contains the ready to install Max package folder into which the externals will be compiled.

### Develop
The project depends on the [aoo](https://aoo.iem.sh/) library and on the [max-sdk-base](https://github.com/Cycling74/max-sdk-base) which are included as submodules in this repository

Clone the repo with:
```bash
git clone https://github.com/ddgg-el/aoo-for-max8.git
git submodule update --init --recursive
```

### Build instruction
From the project's root folder
```bash
$ mkidr build
$ cd build
$ cmake -G<your-generator> .. -DCMAKE_BUILD_TYPE=Release
$ cmake --build . -j${nproc}
```
use `"Xcode"`, `"Unix Makefiles"` or `"Visual Studio <x>"` in place of `<your-generator>`, or simply omit the -G option to use the default one.

The compiled externals will be installed in `package/Aoo for Max/externals`. At this point you are ready to [install](#installation) the project.

### Installation
Manually copy the downlaoded `AOO for Max` folder into your Max `Packages` folder or add it to the Max `Options > File Preferences...`
When building from source you will find this folder inside the  `package` repo subfolder

---

### Reference for Development

Max SDK
[https://sdk.cdn.cycling74.com/max-sdk-8.2.0/index.html](https://sdk.cdn.cycling74.com/max-sdk-8.2.0/index.html)

AOO
[https://aoo.iem.sh/docs/](https://aoo.iem.sh/docs/)
