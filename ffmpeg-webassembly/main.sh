export EXPORTED_FUNCTIONS="[ \
    '_hello', \
'_testPrint', \
'_testa', \
'_testBuffer', \
]"

# --llvm-lto 1
# --closure 1
# --clear-cache
# --ignore-dynamic-linking \
# -g \
#   -g \
#   -s USE_GLFW=3 \
#   --save-bc ./bc \
# -s NO_FILESYSTEM=1



em++ -I./ffmpeg/include main.cpp \
-L./ffmpeg/lib/ -lavformat -lavcodec -lswresample -lmp3lame -lswscale -lx264 -lx265 -lavutil  \
--llvm-lto 0 \
 --ignore-dynamic-linking \
 -Oz  \
  -s WASM=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
   -s RESERVED_FUNCTION_POINTERS=20 \
  -s EXPORTED_FUNCTIONS="${EXPORTED_FUNCTIONS}" \
  -s 'EXTRA_EXPORTED_RUNTIME_METHODS=["ccall", "cwrap","setValue","getValue","addFunction","removeFunction"]' \
  -s EXPORT_NAME="'ffmpeg'" \
  -s MODULARIZE=1 \
	-s DISABLE_EXCEPTION_CATCHING=1 \
  -o main.js