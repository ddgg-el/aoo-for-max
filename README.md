Aoo for Max
===========

### Overview
This repository contains the source code for the `Aoo for Max` package. There is no need to manually build your project. To download a ready to use package please visit the Release page.

The externals has been tested on Max8.1 running on MacOS 12.1 and Windows11. Presumably they should also work with more modern releases.

#### Folder structure
```
├── CMakeLists.txt
├── LICENSE
├── README.md
├── aoo
├── max-sdk-base
├── package
│   └── Aoo for Max <---- the Max package
├── package-info.json
└── source
```
The `source` folder contains the source file for each external while the `package` contains the ready to install Max package folder into which the externals will be compiled.

### Installation
Manually copy the downlaoded `Aoo for Max` folder into your Max `Packages` folder or add it to the Max `Options > File Preferences...`
When building from source you will find this folder inside the  `package` repo subfolder

---

### Develop
The project depends on the [aoo](https://aoo.iem.sh/) library and on the [max-sdk-base](https://github.com/Cycling74/max-sdk-base) which are included as submodules in this repository

Clone the repo with:
```bash
git clone https://github.com/ddgg-el/aoo-for-max8.git
git submodule update --init --recursive
```

**IMPORTANT**
Since Max samples are `double`, as for today you manually have to modify the file `aoo/include/aoo_types.h:100`
```c++
typedef double AooSample; // <---- missing ;
------------------------^
```
Otherwise the project will not compile

#### VSCODE Intellisense Configuration (optional):
```json
"includePath": [
	"${default}",
	"${workspaceFolder}/",
	"${workspaceFolder}/aoo",
	"${workspaceFolder}/aoo/include",
	"${workspaceFolder}/aoo/aoo/src",
	"${workspaceFolder}/aoo/deps/oscpack",
	"${workspaceFolder}/aoo/deps/opus/include",
	"${workspaceFolder}/aoo/deps/portaudio/include",
	"${workspaceFolder}/source/include",
	"${workspaceFolder}/max-sdk-base/c74support/**/",
	// Path to Pd folder which could be different from yours
	"/Applications/Pd-0.54-1.app/Contents/Resources/src/"
],
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

The compiled externals will be installed in `package/Aoo for Max/externals`

### Reference for Development

Max SDK
[https://sdk.cdn.cycling74.com/max-sdk-8.2.0/index.html](https://sdk.cdn.cycling74.com/max-sdk-8.2.0/index.html)

Aoo
[https://aoo.iem.sh/docs/](https://aoo.iem.sh/docs/)
