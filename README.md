# pocketbook_cfa_interference_breaker
Custom module for koreader on Pocketbook to break CFA "Rainbow Effect" on Kaleido 3

This module allows you to display black and white images without the rainbow effect on an e-reader with a Kaleido 3 screen, using Koreader.
It applies "on the fly" via Koreader, so the great benefit of this is that you don't have to alter your original files :)

Disclaimer: It was developed and tested exclusively on the Pocketbook Inkpad Color 3 e-reader, but should be easily portable to other e-readers.

A - Module composition:
- Lua patch "20-apply_cfa_interference_breaker.lua" (invoked in Koreader via the userpatches module):
    - Loads the two libraries "color_detect.so" and "moire_filter_fftw_eco.so"
    - Defines the framebuffer_has_color method, which uses color_detect.so to detect whether the image loaded in the framebuffer is in color or black and white
    - Defines the remove_moire_on_fb method, which removes image frequencies responsible for the appearance of the rainbow effect (interference with the CFA of Kaleido 3 screens)
    - Modifies the "_updateFull", "_updatePartial", or "_updateFast" methods of the pocketbook framebuffer to check whether the image loaded in the framebuffer is in color or black and white, and to apply the removal of patterns responsible for the rainbow effect to black and white images
    - Note: The module loads the resources needed for moire suppression only once when loading the first black and white image, and reuses these resources for subsequent black and white images. These resources are deleted when koreader is exited or when the e-reader is put to sleep.
- "color_detect.so" library (sources are provided in sources/color_detect/ directory)
- "moire_filter_fftw_eco.so" library (sources are provided in sources/moire_filter_fftw_eco/ directory)
  - This library uses FFTW to apply an FFT and then an IFFT to each image. Between the two, a function removes interference.
- libgomp.so.1 library (from gcc compiler) to enable multithreading in libraries

B - Usage on Pocketbook Inkpad Color 3:
  - Copy the content of "modules_for_pocketbook_inkpad_color_3" inside applications/koreader/ on your Pocketbook Inkpad Color 3
  - If necessary, modify the values ​​of the parameters param_radius_min and param_radius_max_diviser in 20-apply_cfa_interference_breaker.lua (increasing "param_radius_min" sharpens the image, and increasing param_radius_max_diviser removes more frequencies from the image, but at too high values, artifacts may appear)

C - Modify the sources for other e-readers and compile
  - The sources/color_detect/ directory contains the sources as well as the makefile I used to modify/compile the library
  - The sources/moire_filter_fftw_eco/ directory contains the sources as well as the makefile I used to modify/compile the library
  - To compile "moire_filter_fftw_eco," you will need to have the libfftw3f.a and libfftw3f_omp.a files in the same directory. To do this, you will need to compile FFTW first (See https://www.fftw.org/download.html)
  - I have attached the instructions I used to compile FFTW in the directory as an example
  - The sources/20-apply_cfa_interference_breaker.lua patch will likely need to be adapted to the possibly different operation of framebuffers other than Pocketbook


I Used gcc-arm-8.3-2019.02-x86_64-arm-linux-gnueabi to cross-compile from Windows WSL, because I think Koreader only allows the load of .so compiled with softfp and not hardfp. See https://developer.arm.com/downloads/-/gnu-a/8-3-2019-02

This project should be greatly improved by users with a better understanding of good computer science practices. You can use my work to make any modifications/improvements that seems useful and allow it to be distributed to as many Kaleido 3 screen users as possible :)

A huge thanks to the FFTW team for their library FFTW 3.3.10 (Matteo Frigo and Steven G. Johnson, “The design and implementation of FFTW3,” Proc. IEEE 93 (2), 216–231 (2005)).
Thanks also to Blendman974 for his idea of ​​using Fourier transforms to remove the rainbow effect on Kaleido 3 !
