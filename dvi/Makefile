binary = dviviewer

sources =					\
	main.cc					\
						\
	painter.cc				\
	font.cc					\
	view.cc					\
	model.cc				\
						\
	dvilib/dl-dvi-file.cc			\
	dvilib/dl-dvi-fontdefinition.cc		\
	dvilib/dl-dvi-parser.cc			\
	dvilib/dl-dvi-program.cc		\
	dvilib/dl-dvi-runtime.cc		\
	dvilib/dl-loader.cc			\
	dvilib/dl-pkfont.cc			\
	dvilib/dl-vffont.cc

headers =					\
	painter.hh				\
	font.hh					\
	view.hh					\
	model.hh				\
						\
	dvilib/dl-dvi-file.hh			\
	dvilib/dl-dvi-fontdefinition.hh		\
	dvilib/dl-dvi-parser.hh			\
	dvilib/dl-dvi-runtime.hh		\
	dvilib/dl-font.hh			\
	dvilib/dl-loader.hh			\
	dvilib/dl-pkfont.hh			\
	dvilib/dl-vffont.hh

CFLAGS = -Idvilib -Wall -W -ansi -pedantic -g -O0 `pkg-config --cflags gtk+-2.0`

$(binary): $(sources) $(headers)
	c++ $(CFLAGS) $(sources) -o$(binary) -lfl `pkg-config --libs gtk+-2.0`
