
Instructions for building Gryptonite
    By: George Karagoulis

Gryptonite was developed using Qt and Crypto++, which are cross-platform and open-source, so
in theory this application will build and run on any platform. That said, I have only tested
it myself on Linux and Windows, but I would be willing to help support other platforms as well.

Before you begin, you will need a python interpreter in the system path, because the build process
leverages custom python scripts. If you want to generate documentation you will also need doxygen.

You will also need Qt version 5 and Crypto++ version 5.6.2 from their website. It is important that
you use that exact version, because I found that later versions are not backwards compatible (even if
it compiles and runs, it doesn't decrypt data correctly using the same calls). You should install
the Crypto++ headers somewhere in the compiler's include path and the libs in a path where the
linker can find it. Building and installing third party libraries is beyond the scope of this document.

After you have the prerequisites you can begin to build. You will use qmake. The easiest way is
through Qt Creator but you can also do it on the command line. If you use Qt Creator you need to disable
the shadow-build feature, because of the way I do headers it won't be able to locate them.

You will build in this order:

    1.) GUtil - Gryptonite depends on my utility library, which is also included in the source. You can
            build it the same way you build any Qt library - qmake; make;
            
    2.) Gryptonite
    
After it builds successfully you should have the executables in the bin directory and the libraries in
lib. I wrote installer setup scripts in the installers directory, so if you want to deploy the application
they will take care of assembling all the files you need.

To create installers:

    Linux - Simply execute ./create_linux_installer.bash from within the installers/linux directory. It
        will grab all the files you need (adjust the script's paths if necessary) and put them in a tarball.
        The deployment includes an install script, "install.bash", which deploys the application on the target.
        
    Windows - I used InnoSetup to generate the installer, so you will need to download and install that freeware.
        After you compile the script you will have a fully functional Windows installer.
