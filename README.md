<h1>Gryptonite (formerly GPassword Manager)</h1>

Secure, cross-platform application for managing secrets such as passwords, credit card
numbers, PIN numbers and files. It can pretty much hold any data you want to keep
secret, and protects it with a single master password and/or keyfile.

Features include (but not limited to):
<ul>
 <li>One of the most secure encryption algorithms around - AES with CCM</li>
 <li>Favorite entries are made available directly in the tray icon</li>
 <li>Hash calculator with all major hash functions</li>
 <li>Crypttext generator and interpreter - Encrypt/Decrypt any strings or files you want</li>
 <li>Random password generator with customizable character set</li>
</ul>


<h3>A note from the developer</h3>

This has been my brainchild since I started it in 2009. I am a computer engineer, and
when I graduated from college I could write embedded code but had no idea how to
implement a GUI. Originally called GPassword Manager, I used this application as a
starter project for me to learn GUI development in C++. I released a couple versions
and then took a long break from the project to work on other things. One day while
revisiting the code for nostalgia purposes I realized just how bad the code was -
despite the application being nice and easy to use. It wasn't just bad coding 
practices (like having no distinguishable layers, or storing pointers in integers...)
but I also noticed the encryption was haphazard and unsecure. I spent the next couple
years completely rearchitecting everything - I practically threw everything out and
started from scratch.

The new application - Gryptonite - is a vast improvement over GPassword Manager in
every aspect. The encryption is MUCH more secure, the database backend is multithreaded
for optimal responsiveness and I fully implemented storing files in the database,
which was not possible in the old database format. I really hope you enjoy the new
version, as this application has been a big part of my life since 2009, both as a
developer and a user.

<br/>
<img src="http://s1.softpedia-static.com/_img/sp100free.png"/>
<br/>

<a href='https://pledgie.com/campaigns/28056' >
    <img alt='Click here to lend your support to: Gryptonite Encrypted Database and make a donation at pledgie.com !' src='https://pledgie.com/campaigns/28056.png?skin_name=chrome' border='0' >
</a>
