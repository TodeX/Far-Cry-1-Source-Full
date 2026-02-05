# Hacks, Workarounds, and Technical Debt

This document catalogs various hacks, workarounds, and instances of technical debt identified in the codebase.

## Explicit Hacks

These are sections of code explicitly marked as hacks by the developers, often indicating temporary solutions or workarounds for critical issues.

*   **Game Connection Synchronization**
    *   **File:** `Editor/GameEngine.cpp`
    *   **Description:** The game update loop is run multiple times (10-20 iterations) in a tight loop to "insure client connection".
    *   **Code:**
        ```cpp
        //@HACK: Update game several times to insure client connection.
        for (int i = 0; i < 10; i++)
        {
            m_IGame->Update();
        }
        ```

*   **AI Attack Range**
    *   **File:** `CryAISystem/Puppet.cpp`
    *   **Description:** A hardcoded check for attack range with an emphatic FIXME comment.
    *   **Code:** `if (fdist < ( m_Parameters.m_fAttackRange/2 ) ) //<<FIXME> HACK!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!`

*   **Entity Deletion**
    *   **File:** `Editor/CryEditDoc.cpp`
    *   **Description:** Forces entity deletion.
    *   **Code:** `//@HACK: this is needed to force delete of entities.`

## Driver & Hardware Workarounds

Specific workarounds for graphics drivers and hardware limitations, largely documented in `RenderDll/Workarounds.txt`.

*   **D3D9 Device Initialization**
    *   **File:** `RenderDll/XRenderD3D9/D3DSystem.cpp`
    *   **Description:** `CreateDevice` is retried up to 20 times with a 50ms sleep between attempts if it fails.

*   **NVidia D3D9 Issues**
    *   **Clip Planes:** Emulated using `texkill` pixel shader instructions because they aren't natively supported.
    *   **Fog:** Calculated in the vertex shader as a workaround.
    *   **Dynamic Textures:** Non-power-of-two dynamic textures fail, so `MANAGED` pool textures are used for video instead of `DEFAULTPOOL`.

*   **ATI OpenGL Issues**
    *   **Performance:** OpenGL driver noted as much slower than Direct3D; D3D renderer is preferred.
    *   **Video Playback:** Disabled on ATI OpenGL due to broken `GL_EXT_texture_rectangle` support.

## Performance Issues & "Sleeps"

Instances where execution is explicitly paused or performance warnings are logged.

*   **Explicit Sleeps**
    *   **File:** `CrySystem/CPUDetect.cpp`
    *   **Description:** `Sleep(100)` is used during CPU frequency detection.
    *   **File:** `FarCry_WinSV/DedicatedServer.cpp`
    *   **Description:** Sleep loop to prevent the dedicated server from consuming 100% CPU.

*   **Performance Warnings**
    *   **File:** `CryAnimation/CryModelAnimationContainer.cpp`
    *   **Description:** Logs a warning about "poor performance upon loading the level" if animation deferred loading is disabled but automatic unload is enabled.

## TODOs and Technical Debt

*   **Leaf Buffer Cleanup**
    *   **File:** `RenderDll/Common/LeafBufferCreate.cpp`
    *   **Code:** `// TODO: get rid of the following`
*   **OpenGL/D3D Copy-Paste**
    *   **File:** `RenderDll/XRenderD3D9/D3DTextures.cpp`
    *   **Description:** Contains a comment `//TODO:replace with ARB_Buffer_Region`, suggesting code was copied from OpenGL implementation to D3D without adjustment.
