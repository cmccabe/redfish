#!/usr/bin/python

class T(object):
    def __init__(self, name, subtrees):
        self.name = name
        self.subtrees = subtrees
        self.depth = None
        self.width = None
    def set_depth_and_width(self, depth, width):
        self.depth = depth
        if (width < 1):
            self.width = 1
        else:
            self.width = width
        for t in self.subtrees:
            t.set_depth_and_width(depth + 1, (int)(width / len(self.subtrees)))
    def stringify_impl(self):
        name_len = len(self.name)
        spos = (int)(self.width - name_len) / 2
        if (spos < 0):
            spos = 0
        outs = ""
        for i in range(0, spos):
            outs = outs + " "
        outs = outs + self.name
        pad = self.width - len(outs)
        if (pad > 0):
            for i in range(0, pad):
                outs = outs + " "
        return outs
    def stringify(self, width):
        self.set_depth_and_width(0, width)
        q = [self]
        cur_depth = 0
        outs = ""
        while True:
            try:
                e = q.pop(0)
                if (e.depth > cur_depth):
                    outs = outs + "\n"
                    cur_depth = e.depth
                for child in e.subtrees:
                    q.append(child)
                outs = outs + e.stringify_impl()
            except IndexError, err:
                break
        return outs

print T("R", [T("A", [T("X", []), T("Y", [])]), T("B", [])]).stringify(79)
