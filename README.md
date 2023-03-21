# Rogue-Robots
Large Game Project at BTH made over a period of 16-weeks.

* Developers: 
  * Nadhif Ginola 
  * Jonatan Hermansson 
  * Emil Fransson 
  * Emil Högstedt 
  * Ove Ødegård 
  * Filip Eriksson 
  * Sam Axelsson 
  * Gunnar Cerne 
  * Oscar Milstein 
  * Axel Lundberg

[![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/F7JwOPsRCII/0.jpg)](https://www.youtube.com/watch?v=F7JwOPsRCII)

My (Nadhif Ginola) responsibilities: 
* __Render backend (Direct3D 12)__
	* Handle based architecture (64 bit uint) for GPU primitives
	* Fully bindless (using Direct Descriptor Access)
  * Minimized API surface area
* __Render Graph__ (Task Graph)
  * Declarative API
  * Automatic barrier insertion
  * Naive memory aliasing
  * String-identified resources
  * Used by other contributors to implement techniques (Forward+, Particles, Bloom, etc.)
* __Render frontend__
  * Data structures (i.e mesh, material, lights)
  * Normal-mapping
  * SSAO
  * Gameplay screen-space effects
  * Assisting other contributors with CPU/GPU data interop. and bindless workflow
  
  
