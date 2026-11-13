#!/usr/bin/env python3
#
# Generates the small test documents used by test/test-ev-*.c.
#
# The generated files are committed, so the test suite does not need any
# of the tools used here.  Run it from any directory:
#
#   python3 test/data/generate.py
#
# Requirements: pycairo, Pillow, qpdf, DjVuLibre (c44, djvm, djvused),
# TeX (tex) and Python's zipfile module.
#
# Every "shapes" document has the same first page, of 200x300 points:
# a white background with a red square at the top left, a green square
# at the top right and a blue square at the bottom right.  The bottom left
# quarter is white.  Raster formats use 1 pixel per point (72 dpi).

import io
import os
import shutil
import subprocess
import tempfile
import zipfile

import cairo
from PIL import Image, ImageDraw

DATA_DIR = os.path.dirname(os.path.abspath(__file__))

WIDTH = 200
HEIGHT = 300

RED = (255, 0, 0)
GREEN = (0, 255, 0)
BLUE = (0, 0, 255)
GREY = (128, 128, 128)

SQUARES = [
    (RED, (20, 20, 80, 80)),
    (GREEN, (120, 20, 180, 80)),
    (BLUE, (120, 220, 180, 280)),
]

PAGE_TEXT = ['Evince page one', 'Evince page two']


def out(name):
    return os.path.join(DATA_DIR, name)


def cairo_draw_page(cr, index):
    cr.set_source_rgb(1, 1, 1)
    cr.paint()

    if index == 0:
        for colour, (x0, y0, x1, y1) in SQUARES:
            cr.set_source_rgb(*[c / 255 for c in colour])
            cr.rectangle(x0, y0, x1 - x0, y1 - y0)
            cr.fill()
    else:
        cr.set_source_rgb(*[c / 255 for c in GREY])
        cr.rectangle(50, 100, 100, 100)
        cr.fill()

    cr.set_source_rgb(0, 0, 0)
    cr.select_font_face('Sans')
    cr.set_font_size(12)
    cr.move_to(20, 150 if index == 0 else 60)
    cr.show_text(PAGE_TEXT[index])


def pil_page(index, scale=1):
    img = Image.new('RGB', (WIDTH * scale, HEIGHT * scale), (255, 255, 255))
    draw = ImageDraw.Draw(img)
    if index == 0:
        for colour, box in SQUARES:
            draw.rectangle([v * scale for v in box[:2]] +
                           [v * scale - 1 for v in box[2:]], fill=colour)
    else:
        draw.rectangle([50 * scale, 100 * scale,
                        150 * scale - 1, 200 * scale - 1], fill=GREY)
    return img


def make_pdf():
    path = out('shapes.pdf')
    surface = cairo.PDFSurface(path, WIDTH, HEIGHT)
    surface.set_metadata(cairo.PDFMetadata.TITLE, 'Evince test document')
    surface.set_metadata(cairo.PDFMetadata.AUTHOR, 'Evince developers')
    surface.set_metadata(cairo.PDFMetadata.SUBJECT, 'Shapes')
    surface.set_metadata(cairo.PDFMetadata.KEYWORDS, 'evince, test')
    surface.set_metadata(cairo.PDFMetadata.CREATOR, 'generate.py')
    surface.set_metadata(cairo.PDFMetadata.CREATE_DATE, '2026-01-01T00:00:00Z')
    surface.set_metadata(cairo.PDFMetadata.MOD_DATE, '2026-01-01T00:00:00Z')
    cr = cairo.Context(surface)

    for index in range(2):
        surface.set_page_label('Cover' if index == 0 else 'Body')
        cairo_draw_page(cr, index)
        if index == 0:
            cr.tag_begin(cairo.TAG_LINK, "dest='second'")
            cr.move_to(20, 190)
            cr.show_text('Go to page two')
            cr.tag_end(cairo.TAG_LINK)
        else:
            cr.tag_begin(cairo.TAG_DEST, "name='second'")
            cr.tag_end(cairo.TAG_DEST)
        surface.add_outline(cairo.PDF_OUTLINE_ROOT,
                            'Chapter %d' % (index + 1),
                            'page=%d pos=[0 0]' % (index + 1), 0)
        cr.show_page()
    surface.finish()


def make_password_pdf():
    subprocess.run(['qpdf', '--encrypt', 'evince', 'owner-evince', '256',
                    '--', out('shapes.pdf'), out('test-password.pdf')],
                   check=True)


def make_ps():
    def rect(colour, box):
        x0, y0, x1, y1 = box
        r, g, b = [c / 255 for c in colour]
        return '%g %g %g setrgbcolor %d %d %d %d rectfill' % (
            r, g, b, x0, HEIGHT - y1, x1 - x0, y1 - y0)

    pages = [
        [rect(c, b) for c, b in SQUARES],
        [rect(GREY, (50, 100, 150, 200))],
    ]
    with open(out('shapes.ps'), 'w') as f:
        f.write('%%!PS-Adobe-3.0\n'
                '%%%%Title: Evince test document\n'
                '%%%%BoundingBox: 0 0 %d %d\n'
                '%%%%DocumentMedia: shapes %d %d 0 () ()\n'
                '%%%%Pages: %d\n'
                '%%%%EndComments\n'
                '%%%%BeginSetup\n'
                '<< /PageSize [%d %d] >> setpagedevice\n'
                '%%%%EndSetup\n' % (WIDTH, HEIGHT, WIDTH, HEIGHT, len(pages),
                                     WIDTH, HEIGHT))
        for index, ops in enumerate(pages):
            f.write('%%%%Page: %d %d\n' % (index + 1, index + 1))
            f.write('\n'.join(ops) + '\nshowpage\n')
        f.write('%%Trailer\n%%EOF\n')


def make_tiff():
    pages = [pil_page(i) for i in range(2)]
    pages[0].save(out('shapes.tiff'), save_all=True, append_images=pages[1:],
                  compression='tiff_adobe_deflate', dpi=(72, 72))


def make_cbz():
    with zipfile.ZipFile(out('shapes.cbz'), 'w', zipfile.ZIP_STORED) as z:
        # Natural sort order must put page-2 before page-10.
        for index, name in enumerate(['page-2.png', 'page-10.png']):
            buf = io.BytesIO()
            pil_page(index).convert('P', palette=Image.ADAPTIVE,
                                    colors=8).save(buf, 'PNG', optimize=True)
            z.writestr(name, buf.getvalue())


def make_djvu():
    tmp = tempfile.mkdtemp()
    try:
        pages = []
        for index in range(2):
            ppm = os.path.join(tmp, 'p%d.ppm' % index)
            djvu = os.path.join(tmp, 'p%d.djvu' % index)
            pil_page(index, 2).save(ppm)
            subprocess.run(['c44', '-dpi', '144', '-slice', '74+10+10',
                            ppm, djvu], check=True)
            words = PAGE_TEXT[index].split()
            x = 40
            txt = []
            for word in words:
                w = len(word) * 14
                txt.append('(word %d 280 %d 304 "%s")' % (x, x + w, word))
                x += w + 10
            sexpr = '(page 0 0 400 600 (line 40 280 %d 304 %s))' % (
                x, ' '.join(txt))
            txtfile = os.path.join(tmp, 'p%d.txt' % index)
            with open(txtfile, 'w') as f:
                f.write(sexpr)
            subprocess.run(['djvused', djvu, '-e',
                            'select 1; set-txt %s' % txtfile, '-s'],
                           check=True)
            pages.append(djvu)
        subprocess.run(['djvm', '-c', out('shapes.djvu')] + pages, check=True)
    finally:
        shutil.rmtree(tmp)


DVI_TEX = r'''
\nopagenumbers
\hsize=200pt \vsize=300pt
\parindent=0pt \topskip=0pt \parskip=0pt
\baselineskip=0pt \lineskip=0pt \lineskiplimit=0pt
\def\square#1#2#3{\special{color push rgb #1}%
  \rlap{\kern#2pt\lower#3pt\hbox{\vrule width 60pt height 0pt depth 60pt}}%
  \special{color pop}}
\shipout\vbox to 300pt{\hbox to 200pt{%
  \square{1 0 0}{20}{20}\square{0 1 0}{120}{20}\square{0 0 1}{120}{220}\hfil}%
  \vfil}
\shipout\vbox to 300pt{\hbox to 200pt{%
  \square{0.5 0.5 0.5}{50}{100}\hfil}\vfil}
\end
'''


def make_dvi():
    tmp = tempfile.mkdtemp()
    try:
        with open(os.path.join(tmp, 'shapes.tex'), 'w') as f:
            f.write(DVI_TEX)
        subprocess.run(['tex', '-interaction=nonstopmode', 'shapes.tex'],
                       cwd=tmp, check=True, stdout=subprocess.DEVNULL)
        shutil.copy(os.path.join(tmp, 'shapes.dvi'), out('shapes.dvi'))
    finally:
        shutil.rmtree(tmp)


XPS_CONTENT_TYPES = '''<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
<Default Extension="fdseq" ContentType="application/vnd.ms-package.xps-fixeddocumentsequence+xml"/>
<Default Extension="fdoc" ContentType="application/vnd.ms-package.xps-fixeddocument+xml"/>
<Default Extension="fpage" ContentType="application/vnd.ms-package.xps-fixedpage+xml"/>
</Types>'''

XPS_RELS = '''<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="R0" Type="http://schemas.microsoft.com/xps/2005/06/fixedrepresentation" Target="/FixedDocSeq.fdseq"/>
</Relationships>'''

XPS_FDSEQ = '''<?xml version="1.0" encoding="UTF-8"?>
<FixedDocumentSequence xmlns="http://schemas.microsoft.com/xps/2005/06">
<DocumentReference Source="/Documents/1/FixedDoc.fdoc"/>
</FixedDocumentSequence>'''

XPS_FDOC = '''<?xml version="1.0" encoding="UTF-8"?>
<FixedDocument xmlns="http://schemas.microsoft.com/xps/2005/06">
<PageContent Source="/Documents/1/Pages/1.fpage"/>
<PageContent Source="/Documents/1/Pages/2.fpage"/>
</FixedDocument>'''


def xps_rect(colour, box):
    x0, y0, x1, y1 = box
    return ('<Path Fill="#FF%02X%02X%02X" Data="M %d,%d L %d,%d %d,%d %d,%d Z"/>'
            % (colour + (x0, y0, x1, y0, x1, y1, x0, y1)))


def make_xps():
    pages = [
        [xps_rect(c, b) for c, b in SQUARES],
        [xps_rect(GREY, (50, 100, 150, 200))],
    ]
    with zipfile.ZipFile(out('shapes.xps'), 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('[Content_Types].xml', XPS_CONTENT_TYPES)
        z.writestr('_rels/.rels', XPS_RELS)
        z.writestr('FixedDocSeq.fdseq', XPS_FDSEQ)
        z.writestr('Documents/1/FixedDoc.fdoc', XPS_FDOC)
        for index, paths in enumerate(pages):
            z.writestr('Documents/1/Pages/%d.fpage' % (index + 1),
                       '<?xml version="1.0" encoding="UTF-8"?>\n'
                       '<FixedPage xmlns="http://schemas.microsoft.com/xps/2005/06" '
                       'Width="%d" Height="%d" xml:lang="en">\n'
                       '<Path Fill="#FFFFFFFF" Data="M 0,0 L %d,0 %d,%d 0,%d Z"/>\n'
                       '%s\n</FixedPage>' % (WIDTH, HEIGHT, WIDTH, WIDTH,
                                             HEIGHT, HEIGHT, '\n'.join(paths)))


class PDFWriter:
    """Minimal PDF writer for features cairo cannot produce."""

    def __init__(self):
        self.objects = []

    def add(self, body):
        if isinstance(body, str):
            body = body.encode('latin-1')
        self.objects.append(body)
        return len(self.objects)

    def reserve(self):
        self.objects.append(None)
        return len(self.objects)

    def set(self, num, body):
        if isinstance(body, str):
            body = body.encode('latin-1')
        self.objects[num - 1] = body

    def stream(self, dictionary, data):
        if isinstance(data, str):
            data = data.encode('latin-1')
        return self.add(b'<< %s /Length %d >>\nstream\n%s\nendstream'
                        % (dictionary.encode('latin-1'), len(data), data))

    def write(self, path, root):
        buf = io.BytesIO()
        buf.write(b'%PDF-1.7\n%\xe2\xe3\xcf\xd3\n')
        offsets = []
        for num, body in enumerate(self.objects, 1):
            offsets.append(buf.tell())
            buf.write(b'%d 0 obj\n%s\nendobj\n' % (num, body))
        xref = buf.tell()
        buf.write(b'xref\n0 %d\n0000000000 65535 f \n' % (len(self.objects) + 1))
        for offset in offsets:
            buf.write(b'%010d 00000 n \n' % offset)
        buf.write(b'trailer\n<< /Size %d /Root %d 0 R >>\nstartxref\n%d\n%%%%EOF\n'
                  % (len(self.objects) + 1, root, xref))
        with open(path, 'wb') as f:
            f.write(buf.getvalue())


def make_features_pdf():
    w = PDFWriter()
    catalog = w.reserve()
    pages = w.reserve()
    page = w.reserve()
    font = w.add('<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica '
                 '/Encoding /WinAnsiEncoding >>')
    content = w.stream('', 'BT /F1 12 Tf 20 260 Td (Features) Tj ET\n'
                           'BT /F1 10 Tf 20 200 Td (Name:) Tj ET\n'
                           'BT /F1 10 Tf 20 160 Td (Agree:) Tj ET')

    note = w.add('<< /Type /Annot /Subtype /Text /Rect [150 250 170 270] '
                 '/Contents (A sticky note) /T (Evince) /NM (note-1) '
                 '/M (D:20260101000000Z) /C [1 1 0] /Name /Comment /F 4 '
                 '/P %d 0 R >>' % page)
    square = w.add('<< /Type /Annot /Subtype /Square /Rect [20 20 80 80] '
                   '/Contents (A square) /T (Evince) /NM (square-1) '
                   '/C [1 0 0] /BS << /W 2 >> /F 4 /P %d 0 R >>' % page)

    text_ap = w.stream('/Type /XObject /Subtype /Form /BBox [0 0 100 20] '
                       '/Resources << /Font << /F1 %d 0 R >> >>' % font,
                       '/Tx BMC BT /F1 10 Tf 2 6 Td (Evince) Tj ET EMC')
    text_field = w.add('<< /Type /Annot /Subtype /Widget /FT /Tx /T (name) '
                       '/V (Evince) /DA (/F1 10 Tf 0 g) /Rect [70 195 170 215] '
                       '/F 4 /P %d 0 R /AP << /N %d 0 R >> >>'
                       % (page, text_ap))
    on_ap = w.stream('/Type /XObject /Subtype /Form /BBox [0 0 15 15]',
                     '0 g 3 3 9 9 re f')
    off_ap = w.stream('/Type /XObject /Subtype /Form /BBox [0 0 15 15]', '')
    check_field = w.add('<< /Type /Annot /Subtype /Widget /FT /Btn /T (agree) '
                        '/V /Yes /AS /Yes /Rect [70 155 85 170] /F 4 '
                        '/P %d 0 R /AP << /N << /Yes %d 0 R /Off %d 0 R >> >> '
                        '>>' % (page, on_ap, off_ap))

    file_data = 'Hello, Evince!\n'
    embedded = w.stream('/Type /EmbeddedFile /Subtype /text#2Fplain '
                        '/Params << /Size %d >>' % len(file_data), file_data)
    filespec = w.add('<< /Type /Filespec /F (hello.txt) /UF (hello.txt) '
                     '/Desc (A text attachment) /EF << /F %d 0 R >> >>'
                     % embedded)

    w.set(page, '<< /Type /Page /Parent %d 0 R /MediaBox [0 0 %d %d] '
                '/Resources << /Font << /F1 %d 0 R >> >> /Contents %d 0 R '
                '/Annots [%d 0 R %d 0 R %d 0 R %d 0 R] >>'
                % (pages, WIDTH, HEIGHT, font, content,
                   note, square, text_field, check_field))
    w.set(pages, '<< /Type /Pages /Kids [%d 0 R] /Count 1 >>' % page)
    w.set(catalog, '<< /Type /Catalog /Pages %d 0 R '
                   '/AcroForm << /Fields [%d 0 R %d 0 R] '
                   '/DR << /Font << /F1 %d 0 R >> >> /DA (/F1 10 Tf 0 g) >> '
                   '/Names << /EmbeddedFiles << /Names [(hello.txt) %d 0 R] >> >> '
                   '>>' % (pages, text_field, check_field, font, filespec))
    w.write(out('test-features.pdf'), catalog)


def main():
    make_pdf()
    make_password_pdf()
    make_features_pdf()
    make_ps()
    make_tiff()
    make_cbz()
    make_djvu()
    make_dvi()
    make_xps()


if __name__ == '__main__':
    main()
