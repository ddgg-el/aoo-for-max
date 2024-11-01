Aoo for Max8
===========

### Sviluppo

Il progetto dipende da [aoo](https://aoo.iem.sh/) e dalla [max-sdk-base](https://github.com/Cycling74/max-sdk-base) che sono inclusi come sottomoduli del progetto. 

Clona la cartella con:
```bash
git clone https://github.com/ddgg-el/aoo-for-max8.git
git submodule init
git submodule update
```

Qualora di volesse implementare il codec `OPUS` e `portaudio` (dipendenze di `aoo`) è possibile aggiungerli alla cartella del progetto a posteriori con il comando
```bash
git submodule update --init --recursive
```

**IMPORTANTE**
Modifica il file `aoo/include/aoo_types.h:100`
```c++
typedef double AooSample; // <---- manca il ;
------------------------^
```

#### Per iniziare
Duplica e rinomina la cartella `source/aoo_template`. Rinomina e modifica il file `simplemsp~.c` usando il nome dell'oggetto sul quale si vuole lavorare. 

Inoltre è utile lavorare su un `branch` separato che verrà poi combinato con `main`.

#### Configurazione linter VSCODE:
```json
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
$ cmake --build . -j${nproc}
```
**All'oggi 1 Novembre 2024 il progetto `aoo.send~` non compila a causa di `aoo_setup.cpp` non ancora implementato**

Gli externals compilati verranno installati nella cartella `externals`

### Verifica
Per verificare che gli oggetti funzionino bisogna aggiungere la cartella `externals` al Path di Max

### Riferimenti

Per gli externals di PD
[https://github.com/pure-data/externals-howto/](https://github.com/pure-data/externals-howto/)