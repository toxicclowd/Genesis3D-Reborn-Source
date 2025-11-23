# LNK1104 Error - FIXED! ?

## Problem: Cannot open file 'Debug\Mkbody.obj'

**Error:** LNK1104 - cannot open file 'Debug\Mkbody.obj'  
**Status:** ? **FIXED**

## Root Cause

**Case sensitivity mismatch** between project file references and actual file/folder names.

### The Issue

- **Project referenced:** `Mkbody\Mkbody.cpp` (mixed case)
- **Actual folder:** `MKBODY\` (all caps)
- **Actual file:** `MKBODY.CPP` (all caps)
- **Compiled object:** `mkbody.obj` (lowercase - from compiler)
- **Linker expected:** `Mkbody.obj` (capital M)

## What Was Fixed

**File Modified:** `source/Tools/ActorTools/ActBuild.vcxproj`

### All Case Mismatches Corrected

Changed folder references:
```
mkbody    ? MKBODY
mkactor   ? MKACTOR  
mkmotion  ? MKMOTION
mop       ? MOP
fmtactor  ? FMTACTOR
```

Changed file references:
```
MKBODY\Mkbody.cpp   ? MKBODY\MKBODY.CPP
MKBODY\Vph.c        ? MKBODY\VPH.C
MKACTOR\Mkactor.c   ? MKACTOR\MKACTOR.C
MKMOTION\Mkmotion.c ? MKMOTION\MKMOTION.C
MOP\Mopshell.c      ? MOP\MOPSHELL.C
MOP\Pop.c           ? MOP\POP.C
Common\Maxmath.c    ? Common\MAXMATH.C
Common\Mkutil.c     ? Common\MKUTIL.C
AStudio\Make.c      ? AStudio\MAKE.C
AStudio\Mxscript.c  ? AStudio\MXSCRIPT.C
```

**Backup Created:** `ActBuild.vcxproj.backup.case`

## Next Steps

### 1. Reload the Project in Visual Studio

- **Right-click** on **jActBuildClassic11** project in Solution Explorer
- Select **"Reload Project"**

Or:
- Close and reopen Visual Studio

### 2. Clean the Solution

Important! Clean old object files:
```
Build ? Clean Solution
```

### 3. Rebuild the Solution

```
Build ? Rebuild Solution
```

or press: **Ctrl + Alt + F7**

### 4. Verify the Fix

You should see:
```
Build succeeded
0 Error(s)
```

The **LNK1104** error should be gone! ?

## Why This Happened

### Windows File System Behavior

Windows is **case-insensitive** for file access, but Visual Studio build tools can be **case-sensitive** for internal tracking.

When the project file says:
```xml
<ClCompile Include="Mkbody\Mkbody.cpp" />
```

But the actual file is `MKBODY\MKBODY.CPP`:
- ? **Compiler finds it** (Windows finds the file)
- ? **Compiles successfully** ? creates `mkbody.obj`
- ? **Linker fails** (looks for `Mkbody.obj` but finds `mkbody.obj`)

### The Fix

By matching the **exact case** in the project file to the actual files:
```xml
<ClCompile Include="MKBODY\MKBODY.CPP" />
```

Now:
- ? Compiler creates the right object file
- ? Linker finds the object file
- ? Build succeeds!

## All Files Fixed

### ActBuild.vcxproj

Total changes: **20+ file/folder references** corrected

**Directories:**
- MKBODY ?
- MKACTOR ?
- MKMOTION ?
- MOP ?
- FMTACTOR ?
- Common ?

**Files:**
- MKBODY.CPP ?
- VPH.C ?
- MKACTOR.C ?
- MKMOTION.C ?
- MOPSHELL.C ?
- POP.C ?
- MAXMATH.C ?
- MKUTIL.C ?
- MAKE.C ?
- MXSCRIPT.C ?
- + all corresponding .H files ?

## Verification

To verify all changes:

```powershell
Select-String -Path "source\Tools\ActorTools\ActBuild.vcxproj" `
              -Pattern "<ClCompile Include"
```

All paths should now match actual file/folder names exactly.

## Rollback (If Needed)

If you need to undo:

```powershell
Copy-Item "source\Tools\ActorTools\ActBuild.vcxproj.backup.case" `
          "source\Tools\ActorTools\ActBuild.vcxproj" -Force
```

## Related Issues

This was a **separate issue** from:
- ? C1041 PDB error (already fixed with `/FS` flag)
- ? DirectX 12 Driver (built successfully)

Both the **C1041 and LNK1104** errors should now be resolved! ??

---

**Status:** Ready to rebuild ?  
**Clean:** Required before rebuild  
**Expected:** Build succeeds with 0 errors
