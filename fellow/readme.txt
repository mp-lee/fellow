Fellow V0.4 Alpha Version Dated 2000.02.06 (Source code archive)
----------------------------------------------------------------

This is a readme file describing the contents of this archive.


Introduction
------------

Like also stated on the Fellow web-page, Fellow is in a discontinued
state. There are, at the time the of making of this archive,
no developers who has time, interest, or due to a combination of the two,
ability to make a substantial contribution that would bring this
project forward within a sensible time-frame.

I (Petter Schau) who created it, and has throughout the project been
its main contributor and maintainer, now think too much time has passed,
and take the liberty of finally closing down the current activities
without making a public version.


Upon getting these sources
--------------------------

The web-page explaining the current state of the project, contained an
offer to receive the source-code on request. This archive contains those
sources.

There are no requirements for receiving these sources.
Do whatever you want with them.
All I ask is that there is no public distribution of this archive or the binary
executable it produces.


GNU Public License (GPL)
------------------------

The source files are tagged with the GNU Public License. Fellow was supposed 
to go GPL, and in the event of a release, Fellow and its source are GPL.
(Consult http://www.gnu.org for information on GPL)


Source organisation
-------------------

src/asm
src/c
src/include
src/incasm

These directories contain what can be refered to as the emulation engine. This is a
generic implementation. Assembly files assemble with the Netwide Assembler (NASM),
the C-files are ANSI-C, although the OS-dependent parts might require a specific
compiler. In the event of changing the C-compiler, some generic header files need to be
changed for the emulation engine files to work with it.

src/uae
src/win32/uae

These directories contain the filesystem module from WinUAE V8.8 and some
other stripped down UAE files needed to interact with it. This module is GPL.
Explicit permission has been granted to use these files from the respective authors.

src/win32/c
src/win32/include
src/win32/incasm

These directories contain pure win32 (and DirectX) implementations needed to 
support Fellow on Win32. A workspace setup to compile Fellow into
an executable is provided for MS Visual Studio (MSVC++ V6, V5 will also work).

src/win32/msvc

This directory contains a MS Visual Studio workspace for the entire Fellow sources.


What you need to compile the sources:
-------------------------------------

MS Visual Studio V6 or V5.
The Netwide Assembler, located in your PATH. (nasmw.exe)
DirectX V? (No, I have not actually examined which version it REALLY needs,
            but NT does not accept anything better than V3, and it works on NT).

Link to NASM: http://www.web-sites.co.uk/nasm/index.html

Other notes:
------------

There is a new configuration format, which was decided ages ago
to be common for UAE and Fellow. Brian King specified most of it.

These is also a keymapping file which allows you to redefine the
mapping of PC-keys to Amiga keys. It is called mapping.key

I think that's enough explaining for now. If anyone wants more detailed
information, I am available on mail (peschau@online.no).

(Such as, how on earth was this supposed to be portable to x86-Linux, anyway?
 Hint: The src/include/*drv.h files declare the interfaces it needs.)



Petter Schau
6. Feb. 2000


