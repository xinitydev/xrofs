#!/usr/bin/env python3

__author__ = 'Xinity/thodnev'
__copyright__ = 'Copyright 2019, Xinity'
__license__ = 'LGPL'
__version__ = '0.1a'

import argparse
import errno
import os
import sys
import struct
import enum
from pathlib import Path, PurePath

Endian = enum.Enum('Endian', 'little big')

_descr = 'Builds xrofs file system image from directory structure'

_epilog = f'''Version: {__version__}
Author: {__author__}
{__copyright__}
This software is licensed under {__license__}'''

class XROFSError(Exception):
    """Base exception class"""
    @property
    def prettystr(self):
        return '{}: {}'.format(self.__class__.__name__, self)

class ImageError(XROFSError):
    pass

class FileError(XROFSError):
    def __init__(self, file, errstr, *args):
        self.file = file
        self.errstr = errstr
        super().__init__(*args)

    def __str__(self):
        return '{} ({}) {}'.format(self.errstr, self.file, self.args or '')

class ArgFormatter(argparse.ArgumentDefaultsHelpFormatter, argparse.RawDescriptionHelpFormatter):
    pass

class ArgParser(argparse.ArgumentParser):
    def __init__(self):
        super().__init__(description=_descr, epilog=_epilog, add_help=False,
                         formatter_class=ArgFormatter)
        self.add_argument('-h', '--help', action='help',
                          help='Display this usage message and exit.')
        self.add_argument('-d', '--dir', type=str, required=True,
                          help='''Directory to be packed into filesystem image''')
        self.add_argument('-f', '--force', action='store_true',
                          help='''Forcefully continue operating on errors''')
        self.add_argument('-o', '--out', type=str, required=True,
                          help='''Generated filesystem image file''')
        self.add_argument('-v', action='store_true',
                          help='''Verbose output''')

    def parse_args(self, args):
        nspace = super().parse_args(args)
        return vars(nspace)



class ImageMaker:
    _isverbose = False
    _isforce = False
    _issilent = True
    _encoding = 'ascii'
    _drive_magic = 0x8000
    _endian = Endian.little

    def __init__(self, dirname, force=None, verbose=None, silent=None,
                 encoding=None, endian=None):
        if verbose is not None:
            self._isverbose = verbose
        if force is not None:
            self._isforce = force
        if silent is not None:
            self._issilent = silent
        if endian is not None:
            self._endian = self._get_endian(endian)
        if encoding is not None:
            'test'.encode(encoding)
            self._encoding = encoding
        self.dirname = dirname
        self.errcnt = 0

    @staticmethod
    def _fobj_copy(src, dst, buffer_len=16*1024):
        cnt = 0
        while 1:
            buf = src.read(buffer_len)
            if not buf:
                break
            cnt += dst.write(buf)
        return cnt

    def _get_endian(self, endian):
        if endian is None:
            endian = self._endian
        elif not isinstance(endian, Endian):
            endian = getattr(Endian, endian)
        return endian

    def mk_drive(self, num_entries, magic=None, endian=None):
        magic = int(magic) if magic is not None else self._drive_magic
        endian = self._get_endian(endian)

        fmt = '<' if endian is Endian.little else '>'
        fmt += 'HH'
        return struct.pack(fmt, magic, num_entries)

    def mk_entry(self, size, offset, endian=None):
        endian = self._get_endian(endian)
        bordstr = {Endian.little: 'little', Endian.big: 'big'}[endian]

        sz = int(size).to_bytes(length=3, byteorder=bordstr)
        off = int(offset).to_bytes(length=4, byteorder=bordstr)
        return sz + off

    @staticmethod
    def human_size(bytesize, short=False):
        sz = bytesize
        if abs(sz) < 1024.0:
            return '%s B ' % sz
        for unit in ['KB','MB','GB']:
            sz /= 1024.0
            if abs(sz) < 1024.0:
                break
        ret = '%4.3f %s' % (sz, unit)
        if not short:
            ret += ' (%s bytes)' % bytesize
        return ret

    def _make_walkerr_handler(self, force):
        def walkerr_handler(error):
            self.errcnt += 1
            e = FileError(error.filename, 'File access error')
            if not force:
                raise e
            print('! ', e.prettystr, file=sys.stderr, sep='')
        return walkerr_handler
        

    def collect_filenames(self, dirname=None, force=None, verbose=None, silent=None):
        dirname = dirname or self.dirname
        verbose = self._isverbose if verbose is None else verbose
        force = self._isforce if force is None else force
        silent = self._issilent if silent is None else silent

        if verbose or not silent:
            print('* Collecting files in "{}"'.format(dirname))

        p = Path(dirname).expanduser()
        if not p.is_dir():
            self.errcnt += 1
            raise FileError(dirname, 'Not a directory')


        files = []
        sizes = []
        hdl = self._make_walkerr_handler(force=force)
        for dirpath, dirnames, filenames in os.walk(p, topdown=False, onerror=hdl):
            f = [Path(dirpath).joinpath(n) for n in filenames]
            
            if not filenames and not dirnames:
                self.errcnt += 1
                msg = 'Useless empty directory ({})'.format(dirpath)
                err = ImageError(msg)
                if force:
                    print('! ', err.prettystr, ' => skipping', file=sys.stderr, sep='')
                    continue
                else:
                    raise err
            
            good_fn = []
            good_sz = []
            for file in f:
                isfile = file.is_file()
                e = ImageError('Not a file ({})'.format(file))
                if not isfile and force:
                    self.errcnt += 1
                    print('! ', e.prettystr, ' => skipping', file=sys.stderr, sep='')
                    continue
                elif not isfile and not force:
                    raise e

                size = file.stat().st_size
                e = ImageError('Useless empty file ({})'.format(file))
                if not size and force:
                    self.errcnt += 1
                    print('! ', e.prettystr, ' => skipping', file=sys.stderr, sep='')
                    continue
                elif not size and not force:
                    raise e

                good_fn.append(file.relative_to(p).as_posix())
                good_sz.append(size)
                    
            if good_fn and verbose:
                s = ''
                for f, sz in zip(good_fn, good_sz):
                    s += '  > {:<64} {:>10}\n'.format(f, self.human_size(sz, short=True))
                print(s, end='')

            files += good_fn
            sizes += good_sz

        if verbose or not silent:
            total = sum(sizes)
            msg = '* Collected {} files totalling {}'
            print(msg.format(len(files), self.human_size(total)))
                
        return dict(zip(files, sizes))


    def write_image(self, namesize_map, imgfile_name, dirname=None, endian=None,
                    verbose=None, encoding=None, force=None):
        dirname = dirname or self.dirname
        endian = self._get_endian(endian)
        verbose = self._isverbose if verbose is None else verbose
        encoding = self._encoding if encoding is None else encoding
        force = self._isforce if force is None else force

        nsmap = dict(namesize_map)
        skeys = sorted(nsmap)
        svals = [nsmap[k] for k in skeys]
        bnames = [k.encode(encoding) + b'\0' for k in skeys]

        header = self.mk_drive(len(nsmap), endian=endian)
        esize = len(self.mk_entry(1, 1, endian=endian))

        eoff = len(header) + esize * len(nsmap)
        entries = []
        offsets = []
        for name, size in zip(bnames, svals):
            e = self.mk_entry(size, eoff, endian=endian)
            offsets.append(eoff)
            entries.append(e)
            eoff += size + len(name)
        header += b''.join(entries)

        msg = '* Writing image "{}" totalling {}'
        print(msg.format(imgfile_name, self.human_size(eoff)))

        total = 0
        dirfile = Path(dirname).expanduser().joinpath
        with open(imgfile_name, mode='wb') as img:
            img.truncate(0)
            total += img.write(header)
            if verbose:
                print('  > [ HEADER ({}) ]'.format(self.human_size(len(header)).rstrip()))
            for fname, bname, size, off in zip(skeys, bnames, svals, offsets):
                if verbose:
                    print('  > {:<64} {:>10}'.format(fname, '@ 0x{:06x}'.format(off)))
                img.seek(off, 0)
                file = open(dirfile(fname), 'rb')
                l = self._fobj_copy(file, img)
                if l != size:
                    self.errcnt += 1
                    e = FileError(str(dirfile(fname)),
                                  'I/O size mismatch. Expected {}, got {}'.format(size, l))
                    if not force:
                        raise e
                    print('! ', e.prettystr, ' => skipping', file=sys.stderr, sep='')
                    continue
                total += l
                total += img.write(bname)

        msg = '* Written {} files {} total [size efficiency: {:.2f} %]'
        print(msg.format(len(skeys),
                         self.human_size(total, short=True).rstrip(),
                         sum(svals) * 100.0 / eoff))

def main(argv):
    args = ArgParser().parse_args(argv[1:])
    mk = ImageMaker(dirname=args['dir'], verbose=args['v'],
                    force=args['force'], silent=False)
    f = mk.collect_filenames()

    mk.write_image(f, args['out'])

    if args['v']:
        errst = 'unstable ({} errors ignored)'.format(mk.errcnt)
        print('Result:', 'good' if not mk.errcnt else errst)
    

if __name__ == '__main__':
    try:
        main(sys.argv)
    except XROFSError as exc:
        print(exc.prettystr, file=sys.stderr)
        exit(errno.EPERM)
    #except Exception as exc:
    #    print("Uncaught exception\n{}: {}".format(exc.__class__.__name__, exc))
    #    exit(errno.EINVAL)
