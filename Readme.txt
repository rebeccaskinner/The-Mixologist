The Mixologist is a communications client that integrates with LibraryMixer.com to form a private network between LibraryMixer.com friends.

To find out more, please visit:
http://www.librarymixer.com/info/mixologist

The Mixologist's sourcecode is split into two parts:
(1) A library portion, which contains all of the actual networking and do-stuff code
(2) A GUI portion, which handles the user interface.

Both portions are written in C++, and utilize the QT framework.

The Mixologist also has external dependencies, and has been configured to look for them in the directory titled ThirdParty.
Under ThirdParty there are two directories, src and lib. 
Both of these directories must be filled in with their expected contents in order to compile the Mixologist.

In order to fill src, you should download the appropriate external libraries and place them in their expected locations.
The expected libraries are described be a Readme file in the src directory.
To make things easier, I've already collected the libraries you will need, and you can find these under Downloads on Github.

In order to fill lib, you can compile the appropriate external libraries and copy them into lib.
Again, the expected libraries are described in a Readme file in the lib directory.
Alternatively, I have precompiled libraries for some platforms on Github under Downloads as well.

Once you have the libraries setup, simply point QT Creator at the two .pro files for MixologistLib and MixologistGui, and then QT Creator provides a fairly straightforward GUI interface to compile the Mixologist.

The distributed version of the Mixologist uses a few more fancy tricks, such as static compiling in order to be able to bundle everything as just one executable. For more information on how to do this, visit the project wiki on Github.
