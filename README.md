Aoo for Max8
===========

### Sviluppo

Il progetto dipende da [Aoo](https://aoo.iem.sh/) e dalla [max-sdk-base](https://github.com/Cycling74/max-sdk-base) che sono inclusi come sottomoduli del progetto. 

#### Configurazione linter VSCODE:
```
"includePath": [
	"${default}",
	"${workspaceFolder}/",
	"${workspaceFolder}/aoo/include",
	"${workspaceFolder}/aoo",
	"${workspaceFolder}/source/include",
	"${workspaceFolder}/max-sdk-base/c74support/**/"
],
```

### Build instruction
```bash
$ mkidr build
$ cd build
$ cmake ..
$ cmake --build . 
```