$!========================================================================
$!
$! Xpdf compile script for VMS.
$!
$! Copyright 1996 Derek B. Noonburg
$!
$!========================================================================
$!
$ XPDF_OBJS = "Array.obj,Catalog.obj,Dict.obj,Error.obj," + -
              "FontEncoding.obj,FontFile.obj,Gfx.obj," + -
              "GfxFont.obj,GfxState.obj,Lexer.obj,Link.obj," + -
              "LTKOutputDev.obj,Object.obj,OutputDev.obj,Page.obj," + -
              "Params.obj,Parser.obj,PDFDoc.obj,PSOutputDev.obj," + -
              "Stream.obj,TextOutputDev.obj,XOutputDev.obj,XRef.obj"
$ XPDF_LIBS = "[-.ltk]libltk.olb/lib,[-.goo]libgoo.olb/lib"
$!
$ PDFTOPS_OBJS = "Array.obj,Catalog.obj,Dict.obj,Error.obj," + -
                 "FontEncoding.obj,FontFile.obj,Gfx.obj," + -
                 "GfxFont.obj,GfxState.obj,Lexer.obj,Link.obj," + -
                 "Object.obj,OutputDev.obj,Page.obj,Params.obj," + -
                 "Parser.obj,PDFdoc.obj,PSOutputDev.obj,Stream.obj," + -
                 "XRef.obj"
$ PDFTOPS_LIBS = "[-.goo]libgoo.olb/lib"
$!
$ PDFTOTEXT_OBJS = "Array.obj,Catalog.obj,Dict.obj,Error.obj," + -
                   "FontEncoding.obj,FontFile.obj,Gfx.obj," + -
                   "GfxFont.obj,GfxState.obj,Lexer.obj,Link.obj," + -
                   "Object.obj,OutputDev.obj,Page.obj,Params.obj," + -
                   "Parser.obj,PDFdoc.obj,TextOutputDev.obj,Stream.obj," + -
                   "XRef.obj"
$ PDFTOTEXT_LIBS = "[-.goo]libgoo.olb/lib"
$!
$ PDFINFO_OBJS = "Array.obj,Catalog.obj,Dict.obj,Error.obj," + -
                 "FontEncoding.obj,FontFile.obj,Gfx.obj," + -
                 "GfxFont.obj,GfxState.obj,Lexer.obj,Link.obj," + -
                 "Object.obj,OutputDev.obj,Page.obj,Params.obj," + -
                 "Parser.obj,PDFdoc.obj,Stream.obj,XRef.obj"
$ PDFINFO_LIBS = "[-.goo]libgoo.olb/lib"
$!
$ PDFTOPBM_OBJS = "Array.obj,Catalog.obj,Dict.obj,Error.obj," + -
                  "FontEncoding.obj,FontFile.obj,Gfx.obj," + -
                  "GfxFont.obj,GfxState.obj,Lexer.obj,Link.obj," + -
                  "Object.obj,OutputDev.obj,PBMOutputDev.obj,Page.obj," + -
                  "Params.obj,Parser.obj,PDFdoc.obj,Stream.obj," + -
                  "TextOutputDev.obj,XOutputDev.obj,XRef.obj"
$ PDFTOPBM_LIBS = "[-.goo]libgoo.olb/lib"
$!
$ PDFIMAGES_OBJS = "Array.obj,Catalog.obj,Dict.obj,Error.obj," + -
                   "FontEncoding.obj,FontFile.obj,Gfx.obj," + -
                   "GfxFont.obj,GfxState.obj,ImageOutputDev.obj," + -
                   "Lexer.obj,Link.obj,Object.obj,OutputDev.obj,Page.obj," + -
                   "Params.obj,Parser.obj,PDFdoc.obj,Stream.obj,XRef.obj"
$ PDFIMAGES_LIBS = "[-.goo]libgoo.olb/lib"
$! Build xpdf-ltk.h
$ def/user sys$input xpdf.ltk
$ def/user sys$output xpdf-ltk.h
$ run [-.ltk]ltkbuild
$!
$ CXXCOMP Array.cc
$ CXXCOMP Catalog.cc
$ CXXCOMP Dict.cc
$ CXXCOMP Error.cc
$ CXXCOMP FontEncoding.cc
$ CXXCOMP FontFile.cc
$ CXXCOMP Gfx.cc
$ CXXCOMP GfxFont.cc
$ CXXCOMP GfxState.cc
$ CXXCOMP ImageOutputDev.cc
$ CXXCOMP Lexer.cc
$ CXXCOMP Link.cc
$ CXXCOMP LTKOutputDev.cc
$ CXXCOMP Object.cc
$ CXXCOMP OutputDev.cc
$ CXXCOMP Page.cc
$ CXXCOMP Params.cc
$ CXXCOMP Parser.cc
$ CXXCOMP PBMOutputDev.cc
$ CXXCOMP PDFDoc.cc
$ CXXCOMP PSOutputDev.cc
$ CXXCOMP Stream.cc
$ CXXCOMP TextOutputDev.cc
$ CXXCOMP XOutputDev.cc
$ CXXCOMP XRef.cc
$ CXXCOMP xpdf.cc
$ CXXCOMP pdftops.cc
$ CXXCOMP pdftotext.cc
$ CXXCOMP pdfinfo.cc
$ CXXCOMP pdftopbm.cc
$ CXXCOMP pdfimages.cc
$!
$ link xpdf,'XPDF_OBJS,'XPDF_LIBS,[-]xpdf.opt/opt
$ link pdftops,'PDFTOPS_OBJS,'PDFTOPS_LIBS,[-]xpdf.opt/opt
$ link pdftotext,'PDFTOTEXT_OBJS,'PDFTOTEXT_LIBS,[-]xpdf.opt/opt
$ link pdfinfo,'PDFINFO_OBJS,'PDFINFO_LIBS,[-]xpdf.opt/opt
$ link pdftopbm,'PDFTOPBM_OBJS,'PDFTOPBM_LIBS,[-]xpdf.opt/opt
$ link pdfimages,'PDFIMAGES_OBJS,'PDFIMAGES_LIBS,[-]xpdf.opt/opt
