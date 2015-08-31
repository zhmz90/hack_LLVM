kaleidoscope: lexer_parser_ast_cgen.cpp
	clang++ -g lexer_parser_ast_cgen.cpp `llvm-config-3.3 --cxxflags --ldflags --system-libs --libs core mcjit native` -O3 \
	-o kaleidoscope -rdynamic -v
