# IPC portable C - memoire partagee sequentielle

Petit projet de demonstration pour remplacer un transport TCP sequentiel par une
memoire partagee binaire, tout en gardant une logique `send(buffer, size)` / 
`receive(buffer, capacity, &received_size)`.

## Architecture

- `include/ipc_transport.h` : API publique de transport.
- `include/ipc_protocol.h` : helper facultatif `API code + taille + payload`.
- `src/ipc_transport.c` : logique commune client/serveur.
- `src/ipc_platform_posix.c` : Linux (`shm_open`, `mmap`, semaphores POSIX).
- `src/ipc_platform_win32.c` : Windows (`CreateFileMapping` sur le pagefile,
  `MapViewOfFile`, events nommes). Aucun fichier utilisateur n'est cree.
- `examples/` : client et serveur de test.

Le serveur cree le canal. Le client s'y connecte. Le protocole est strictement
sequentiel :

1. client `send`
2. serveur `receive`
3. serveur `send`
4. client `receive`
5. nouvel echange

Il n'y a qu'un seul buffer partage.

## Compatibilite 32 / 64 bits

Les structures de protocole utilisent des types a taille fixe (`uint32_t`,
`int32_t`, `float`) et ne contiennent aucun pointeur, `size_t` ou `long`.
Des `_Static_assert` verifient les tailles importantes a la compilation.

Pour un protocole destine a d'autres architectures que x86/x64, il faudra aussi
normaliser explicitement l'endianness et, si necessaire, la representation des
flottants.

## Linux

### Compilation simple

```bash
make
```

Puis dans deux terminaux :

```bash
./ipc_server
```

```bash
./ipc_client
```

### CMake

```bash
cmake -S . -B build
cmake --build build
./build/ipc_server
./build/ipc_client
```

Pour tester un executable 32 bits sous Debian/Ubuntu, installer les bibliotheques
multilib puis ajouter `-m32` a la compilation de cet executable.

## Windows

Avec Visual Studio Developer Command Prompt :

```bat
cmake -S . -B build
cmake --build build --config Release
```

Puis lancer d'abord `ipc_server.exe`, puis `ipc_client.exe`.

Pour avoir un processus 32 bits et l'autre 64 bits, genere deux repertoires de
build CMake, l'un avec le generateur/plateforme Win32 et l'autre avec x64. Les
objets Windows nommes sont accessibles aux deux processus de la meme session.

## API principale

```c
ipc_transport_open(...);
ipc_transport_send(ipc, buffer, size);
ipc_transport_receive(ipc, buffer, capacity, &received, timeout_ms);
ipc_transport_close(ipc);
```

La couche transport ne connait pas le contenu du message. `ipc_protocol` est un
exemple de couche superieure avec :

```text
uint32_t api_code
uint32_t payload_size
payload[payload_size]
```

Tu peux remplacer cette couche par ton propre format sans modifier le transport.
