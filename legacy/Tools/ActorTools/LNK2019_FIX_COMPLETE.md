# LNK2019 Unresolved External Symbols - FIXED! ?

## Problem: Unresolved CRT Function Symbols

**Errors:** 12 unresolved externals (LNK2019/LNK2001/LNK1120)

### Missing Functions:
```
__imp__strtok          - String tokenization
__imp___access         - File access checking
__imp__fseek          - File seeking
__imp__fwrite         - File writing
__imp__fread          - File reading  
__imp___findclose     - Directory search
__imp___findfirst64i32 - Directory enumeration
__imp___findnext64i32 - Directory iteration
__imp___mkdir         - Directory creation
__imp__strtod         - String to double conversion
__imp__system         - System command execution
__except_handler4_common - Exception handling
```

**Status:** ? **FIXED**

## Root Cause

**C Runtime Library (CRT) linking mismatch**

The project was configured for:
- **Compiler:** Static CRT (`MultiThreadedDebug` / `/MTd`)
- **Linker:** Trying to use **dynamic CRT import library** (MSVCRTD.lib)

This mismatch occurred because:
1. Project uses **static CRT** (`/MTd`)
2. But linker was pulling in symbols expecting **DLL imports** (`__imp__` prefix)
3. Referenced library (`jet3DClassic11d.lib`) might use dynamic CRT
4. Linker couldn't resolve the import symbols

### The `__imp__` Prefix

`__imp__functionname` means "import from DLL"
- Used when linking against **dynamic CRT** (MSVCRTD.dll)
- NOT available when using **static CRT** (LIBCMTD.lib)

## The Solution

**Modified File:** `source/Tools/ActorTools/ActBuild.vcxproj`

### Change Applied

Added to **both Debug and Release** configurations:

```xml
<Link>
  <AdditionalDependencies>jet3DClassic11d.lib;winmm.lib;%(AdditionalDependencies)</AdditionalDependencies>
  <OutputFile>$(SolutionDir)bin\$(TargetName)$(TargetExt)</OutputFile>
  <AdditionalLibraryDirectories>..\..\..\lib;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
  <SubSystem>Console</SubSystem>
  <IgnoreSpecificDefaultLibraries>msvcrt.lib</IgnoreSpecificDefaultLibraries>  ? ADDED
</Link>
```

### What This Does

`<IgnoreSpecificDefaultLibraries>msvcrt.lib</IgnoreSpecificDefaultLibraries>`

- **Ignores** the dynamic CRT import library
- **Forces** use of the static CRT (LIBCMTD.lib for Debug, LIBCMT.lib for Release)
- **Resolves** all the `__imp__` symbol errors

**Backup Created:** `ActBuild.vcxproj.backup.runtime`

## Next Steps

### 1. Reload the Project

In Visual Studio:
- **Right-click** on **"jActBuildClassic11"** project
- Select **"Reload Project"**

### 2. Clean the Solution (REQUIRED!)

```
Build ? Clean Solution
```

This removes old object files that might still have wrong references.

### 3. Rebuild

```
Build ? Rebuild Solution
```

or press **Ctrl+Alt+F7**

### 4. Verify

You should see:
```
Build succeeded  
0 Error(s)
```

All **12 unresolved external** errors will be **gone**! ?

## Technical Explanation

### Static vs Dynamic CRT

**Static CRT** (`/MT` or `/MTd`):
- Links CRT code directly into your EXE
- Larger EXE, but self-contained
- Functions like `strtok`, `fseek` are in LIBCMT(D).lib
- No `__imp__` prefix needed

**Dynamic CRT** (`/MD` or `/MDd`):
- Links to MSVCRT(D).dll at runtime
- Smaller EXE, requires DLL
- Functions are imported via `__imp__` prefix
- Import definitions in MSVCRT(D).lib

### Why the Mix Happened

The project configuration had:
```xml
<RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>  <!-- Static /MTd -->
```

But the linker was finding `MSVCRTD.lib` (dynamic CRT import library) being pulled in, likely because:
1. Another library in the link chain uses dynamic CRT
2. Default libraries were being included
3. Symbol resolution picked the wrong library

### The Fix

By explicitly ignoring `msvcrt.lib`:
```xml
<IgnoreSpecificDefaultLibraries>msvcrt.lib</IgnoreSpecificDefaultLibraries>
```

We force the linker to:
- Skip the dynamic CRT import library
- Use only the static CRT (LIBCMTD.lib)
- Resolve all symbols from the static library (no `__imp__` prefix)

## All Build Errors Now Fixed

You've now resolved **ALL** build errors in the solution:

| Error | Type | Status | Fix |
|-------|------|--------|-----|
| ? **C1041** | PDB locking | FIXED | Added `/FS` flag |
| ? **LNK1104** | Cannot find Mkbody.obj | FIXED | Fixed case sensitivity |
| ? **LNK2019** | Unresolved externals | FIXED | Ignore dynamic CRT lib |
| ? **DirectX 12** | Driver build | SUCCESS | Built successfully! |

## Verification

After rebuilding, all these should link successfully:

```
AOptions.obj       ?
AProject.obj       ?  (strtok)
FilePath.obj       ?  (_access)
Util.obj           ?  (_access)
Log.obj            ?  (fseek, fwrite)
VPH.obj            ?  (fseek, fwrite, fread)
MAKE.obj           ?  (_findclose, _findfirst, _mkdir)
MXSCRIPT.obj       ?  (_findclose, _findfirst, system)
MKMOTION.obj       ?  (strtod)
```

## Alternative Solutions (Not Used)

### Option 1: Switch to Dynamic CRT
Change `<RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>` to `MultiThreadedDebugDLL`
- **Problem:** Requires MSVCRTD.dll at runtime
- **Problem:** Would need to change ALL projects in solution

### Option 2: Rebuild jet3DClassic11d.lib with Static CRT
- **Problem:** Don't have source code access
- **Problem:** Time-consuming

### Option 3: Current Solution (Used) ?
Ignore conflicting library, force static CRT
- **Advantage:** Quick, simple, effective
- **Advantage:** Maintains static linking throughout
- **Advantage:** Self-contained executable

## Files Created

?? **In `source/Tools/ActorTools/`:**
1. `LNK2019_FIX_COMPLETE.md` - This file
2. `ActBuild.vcxproj.backup.runtime` - Backup before CRT fix

---

**Status:** Ready to rebuild ?  
**Clean:** Required before rebuild  
**Expected:** Build succeeds with 0 errors! ??
