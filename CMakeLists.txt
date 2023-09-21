cmake_minimum_required(VERSION 3.27)
project(15_466_f23_base3)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(15_466_f23_base3
        ColorProgram.cpp
        ColorProgram.hpp
        ColorTextureProgram.cpp
        ColorTextureProgram.hpp
        data_path.cpp
        data_path.hpp
        DrawLines.cpp
        DrawLines.hpp
        GL.cpp
        GL.hpp
        gl_compile_program.cpp
        gl_compile_program.hpp
        gl_errors.hpp
        glcorearb.h
        LitColorTextureProgram.cpp
        LitColorTextureProgram.hpp
        Load.cpp
        Load.hpp
        load_opus.cpp
        load_opus.hpp
        load_save_png.cpp
        load_save_png.hpp
        load_wav.cpp
        load_wav.hpp
        main.cpp
        Mesh.cpp
        Mesh.hpp
        Mode.cpp
        Mode.hpp
        PathFont-font.cpp
        PathFont.cpp
        PathFont.hpp
        PlayMode.cpp
        PlayMode.hpp
        read_write_chunk.hpp
        Scene.cpp
        Scene.hpp
        show-meshes.cpp
        show-scene.cpp
        ShowMeshesMode.cpp
        ShowMeshesMode.hpp
        ShowMeshesProgram.cpp
        ShowMeshesProgram.hpp
        ShowSceneMode.cpp
        ShowSceneMode.hpp
        ShowSceneProgram.cpp
        ShowSceneProgram.hpp
        Sound.cpp
        Sound.hpp)
