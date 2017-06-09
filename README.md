Pool2d
======

Box2D-based pool game, proof of concept. Obviously, it's possible.

Objects should be properly initialized, gravity should be set to zero. For example:

* fd.friction = 0.99f; // fixes friction
* fd.restitution = 0.5f; // enables bouncing
* bd.linearDamping = 0.5f; // reduces simulation time
* bd.angularDamping = FLT_MAX; // prevents rotation
* bd.bullet = true; // enables CCD

