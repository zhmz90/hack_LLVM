test: lexer_parser_ast_cgen.cpp
	clang++ lexer_parser_ast_cgen.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core mcjit native`  \
	-o kaleidoscope 
kaleidoscope: lexer_parser_ast_cgen.cpp
	clang++ -g lexer_parser_ast_cgen.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core mcjit native` -O3 \
	-o kaleidoscope -v
