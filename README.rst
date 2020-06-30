XROFS – Xinity Read-Only FileSystem
===================================

XROFS is a tiny library providing basic in-memory filesystem functionality for small constrained
embedded devices.

The simplest recipe usually proposed on the internet for adding binary files to firmare is to 
store them as arrays of bytes.
Have you been pushing the hell out of these C files dreaming of something more convenient, but 
still not as bloated as full-featured flash filesystems? We too. And here's what we ended up with.

XROFS Features:
  - *No dynamic memory allocations at all*
    
    Use provided tool to generate a binary image for the directories and files.
    
    Then direct linker towards adding this single image to your firmware.
    
    Define image start address as ``xrofs_dev`` in one of the headers
    
    And that's all what is needed to work with files inside the image

  - *High storage efficiency*
    
    In our tests on ~real~ files generally present in embedded projects showed more than 
    **99.5 %** storage utilization efficiency

  - *Small library footprint*
    
    Just take a look at `xrofs.c` and `xrofs.h` sources. It's all what you need to use XROFS
    in your embedded project

  - *Fast execution speed*
    
    We store files linearly, which allows for the same high read spead, as for the simple byte 
    arrays.
    
    File metadata is stored in presorted, binary search optimized order, so lookups at file
    ``open()`` are screamingly fast.

  - *"Standard" file I/O interface*
    
    Library mimics corresponding I/O stuff, which is usually found in stdio.
    
    But You may also like to use ``map`` for using it as a drop-in replacement for the old known 
    "binary array" files.

Distributed under `Xinity License <https://github.com/xinitydev/xlicense>`_ (see `<LICENSE.txt>`_ for details)
