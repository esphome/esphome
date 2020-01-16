""" Read Gimp .ggr gradient files.
    Ned Batchelder, http://nedbatchelder.com
    This code is in the public domain.

    Adjusted for esphome by Daniel Poelzleithner <git@poelzi.org>
"""

__version__ = '1.0.20070915'

import colorsys, math

class GimpGradient(object):
    """ Read and interpret a Gimp .ggr gradient file.
    """
    def __init__(self, f=None, content=None):
        self.name = None
        if content:
            self.parse_content(content)
        if f:
            self.read(f)
        
    class _segment:
        pass
    
    def read(self, f):
        """ Read a .ggr file from f (either an open file or a file path).
        """
        if isinstance(f, basestring):
            f = file(f)
        if f.readline().strip() != "GIMP Gradient":
            raise Exception("Not a GIMP gradient file")
        content = []
        for line in f.readline():
            content.append(line)

    def parse_content(self, data = None):
        content = [x.strip() for x in data.splitlines() if x]
        self.parse_lines(content)

    def parse_lines(self, content=[]):
        print(content)
        i = 0
        nseq = None
        if content[i].startswith("GIMP Gradient"):
            i += 1
        if content[i].startswith("Name: "):
            self.name = content[i].split(": ", 1)[1]
            i += 1
        try:
            nsegs = int(content[i].strip())
            i += 1
        except ValueError as e:
            pass
        self.segs = []
        for line in content[i:nseq and nseq + i or None]:
            line = line.strip()
            if not line:
                continue
            seg = self._segment()
            (seg.l, seg.m, seg.r,
                seg.rl, seg.gl, seg.bl, _,
                seg.rr, seg.gr, seg.br, _,
                seg.fn, seg.space) = map(float, line.split())
            self.segs.append(seg)
            
    def color(self, x):
        """ Get the color for the point x in the range [0..1).
            The color is returned as an rgb triple, with all values in the range
            [0..1).
        """
        # Find the segment.
        for seg in self.segs:
            if seg.l <= x <= seg.r:
                break
        else:
            # No segment applies! Return black I guess.
            return (0,0,0)

        # Normalize the segment geometry.
        mid = (seg.m - seg.l)/(seg.r - seg.l)
        pos = (x - seg.l)/(seg.r - seg.l)
        
        # Assume linear (most common, and needed by most others).
        if pos <= mid:
            f = pos/mid/2
        else:
            f = (pos - mid)/(1 - mid)/2 + 0.5

        # Find the correct interpolation factor.
        if seg.fn == 1:   # Curved
            f = math.pow(pos, math.log(0.5) / math.log(mid));
        elif seg.fn == 2:   # Sinusoidal
            f = (math.sin((-math.pi/2) + math.pi*f) + 1)/2
        elif seg.fn == 3:   # Spherical increasing
            f -= 1
            f = math.sqrt(1 - f*f)
        elif seg.fn == 4:   # Spherical decreasing
            f = 1 - math.sqrt(1 - f*f);

        # Interpolate the colors
        if seg.space == 0:
            c = (
                seg.rl + (seg.rr-seg.rl) * f,
                seg.gl + (seg.gr-seg.gl) * f,
                seg.bl + (seg.br-seg.bl) * f
                )
        elif seg.space in (1,2):
            hl, sl, vl = colorsys.rgb_to_hsv(seg.rl, seg.gl, seg.bl)
            hr, sr, vr = colorsys.rgb_to_hsv(seg.rr, seg.gr, seg.br)

            if seg.space == 1 and hr < hl:
                hr += 1
            elif seg.space == 2 and hr > hl:
                hr -= 1

            c = colorsys.hsv_to_rgb(
                (hl + (hr-hl) * f) % 1.0,
                sl + (sr-sl) * f,
                vl + (vr-vl) * f
                )
        return c
    
if __name__ == '__main__':
    
    test = """
    GIMP Gradient
    Name: Tropical Colors
    9
    0.000000 0.055556 0.085142 0.036578 0.159091 0.015374 1.000000 0.007899 0.310606 0.000000 1.000000 0 0
    0.085142 0.138564 0.193656 0.007899 0.310606 0.000000 1.000000 0.195893 0.575758 0.085655 1.000000 0 0
    0.193656 0.233723 0.276572 0.195893 0.575758 0.085655 1.000000 0.924242 0.750598 0.192395 1.000000 0 0
    0.276572 0.332128 0.387683 0.924242 0.750598 0.192395 1.000000 0.954545 0.239854 0.132221 1.000000 0 0
    0.387683 0.510851 0.555556 0.954545 0.239854 0.132221 1.000000 0.530303 0.319349 0.236012 1.000000 0 0
    0.555556 0.611111 0.666667 0.530303 0.319349 0.236012 1.000000 0.472649 0.295792 1.000000 1.000000 0 0
    0.666667 0.772955 0.826377 0.472649 0.295792 1.000000 1.000000 0.644153 1.000000 0.957743 1.000000 0 0
    0.826377 0.866444 0.884808 0.644153 1.000000 0.957743 1.000000 0.408723 0.870000 0.278400 1.000000 0 0
    0.884808 0.953255 1.000000 0.408723 0.870000 0.278400 1.000000 0.363558 0.500000 0.000000 1.000000 1 0
    """

    gg1 = GimpGradient(content=test)
    print(gg1.color(0))
    print(gg1.color(0.5))
    print(gg1.color(1.0))
    
    
    #import sys, wx

    # class GgrView(wx.Frame):
    #     def __init__(self, ggr, chunks):
    #         """ Display the ggr file as a strip of colors.
    #             If chunks is non-zero, then also display the gradient quantized
    #             into that many chunks.
    #         """
    #         super(GgrView, self).__init__(None, -1, 'Ggr: %s' % ggr.name)
    #         self.ggr = ggr
    #         self.chunks = chunks
    #         self.SetSize((600, 100))
    #         self.panel = wx.Panel(self)
    #         self.panel.Bind(wx.EVT_PAINT, self.on_paint)
    #         self.panel.Bind(wx.EVT_SIZE, self.on_size)

    #     def on_paint(self, event):
    #         dc = wx.PaintDC(self.panel)
    #         cw, ch = self.GetClientSize()
    #         if self.chunks:
    #             self.paint_some(dc, 0, 0, ch/2)
    #             self.paint_some(dc, self.chunks, ch/2, ch)
    #         else:
    #             self.paint_some(dc, 0, 0, ch)
                
    #     def paint_some(self, dc, chunks, y0, y1):
    #         cw, ch = self.GetClientSize()
    #         chunkw = 1
    #         if chunks:
    #             chunkw = (cw // chunks) or 1
    #         for x in range(0, cw, chunkw):
    #             c = map(lambda x:int(255*x), ggr.color(float(x)/cw))
    #             dc.SetPen(wx.Pen(wx.Colour(*c), 1))
    #             dc.SetBrush(wx.Brush(wx.Colour(*c), wx.SOLID))
    #             dc.DrawRectangle(x, y0, chunkw, y1-y0)
        
    #     def on_size(self, event):
    #         self.Refresh()

    # app = wx.PySimpleApp()
    # ggr = GimpGradient(sys.argv[1])
    # chunks = 0
    # if len(sys.argv) > 2:
    #     chunks = int(sys.argv[2])
    # f = GgrView(ggr, chunks)
    # f.Show()
    # app.MainLoop()
