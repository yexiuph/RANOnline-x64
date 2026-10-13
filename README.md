# Ran Online

Ran Online (stylized as RAN Online) was a massively multiplayer online role-playing game (MMORPG) developed by [Min Communications, Inc](http://www.mincoms.co.kr/). The game is set in real-life schools and cities, where players take the role of students in three different school clubs gathering to protect different areas from evil forces. The game offers different character classes, PvP, and more.

## Disclaimers

This source code is based on EP7 shared by Jollin88. All copyrights to this source code are owned by [Min Communications, Inc](http://www.mincoms.co.kr/) and all their respective developers. All modifications made in this source code are only for educational purposes, and if you want to make a private server using this source code, please do so at your own risk. I will not be responsible for anything you do with this source code.

You must be fully aware and accept that every code you work on in this project belongs to the community (which is a member of RaGEZONE). You (as a member) cannot sue members in this project including me (as the owner) for any reason because I (as the owner) or others, have never encouraged or asked you to work on this project.

This source code is fully free to download and to use. We have [Discord Server!](https://discord.gg/VfH7xQc) feel free to join.

## Goals

### Priority

- Make this source code as stable as possible.
- Clean this source code as clean as possible.
- Update sources to new C ++ standards and syntax (11, 14, 17).
- Completing tools (WorldEditor, EffectEditor, and so on).

### Nice To Have

- Update all dependencies to new version (DirectX, and so on).
- Adding support for Shader.
- Adding support for NVIDIA PhysX Library.
- Adding support for LUA script.

This source code will remain in EP7 until I decide to update it to a newer EP. So, don't bother asking me about new features.

## Code of Conduct

**IMPORTANT** : I will not merge your works if you do not follow my rules.

- Create branch from branch master for each tasks / issues that you want to work (one branch, one task / issue).
- The new branch name should be same as task / issue title.
- Provide details for every commit you push.
- Provide change log in every merge request detail.

## How to Contribute

- Must have knowledge in C++.
- Willing to work for the community (for free of course).
- Must have Gitlab account.
- Have a forum RaGEZONE account.

## Contributors

- [geger009](http://forum.ragezone.com/members/1333334837.html)
- [Quiirex](http://forum.ragezone.com/members/1333385074.html)
- [lars](http://forum.ragezone.com/members/2000202916.html)

## Building

Open `RanOnline.sln` in Visual Studio 2022 (toolset v143) and pick a configuration (`Debug`, `Release`, `Release_RZ`) and a platform (`Win32` or `x64`).

- **vcpkg** supplies Assimp on both platforms and the x64 builds of lzo, ogg and vorbis (see `vcpkg.json`). The packages are installed automatically on the first build. Set `VCPKG_ROOT` to your vcpkg checkout, or leave it unset to use the vcpkg bundled with Visual Studio.
- **Output**: Win32 programs go to `..\game-client\`, x64 programs to `..\game-client\x64\`, together with the vcpkg DLLs they need.
- **Runtime**: the x64 client needs the 64-bit `d3dx9_26.dll` from the DirectX End-User Runtime (June 2010). `d3dxof.dll` is no longer used; skeletons are read by YXCore.
- Win32 and x64 programs read the same data files and speak the same network protocol.

### YXCore

`YXCore` holds the model loading that replaces the parts of DirectX 9 that don't exist on x64:

- `yx::LoadFrameHierarchy` reads skeleton frame hierarchies. `.x` files are read by YXCore's own reader so the bones match DirectX bit for bit; other formats go through Assimp.
- `yx::LoadMeshHierarchy` is an Assimp-based replacement for `D3DXLoadMeshHierarchyFromX`. The engine uses it for skins in formats other than `.x` (FBX, glTF, OBJ, DAE, ...). `.x` skins stay on D3DX, because trace points in `.cps` pieces and the `.enm`/`.ehs`/`.nor` files store vertex indices that depend on D3DX's vertex order.

`YXModelVerify` checks those claims against real client data:

```bat
YXModelVerify "<client>\data\skeleton" "<client>\data\skin"         rem frame hierarchies vs DirectX
YXModelVerify --meshes "<client>\data\skin"                          rem skins: D3DX vs Assimp
```

# Game Client

[Ran Online Client Based on PH V822](https://gitlab.com/ragezone/ranonline/game-client)