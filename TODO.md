### Overview

 was that the package could use a richer "Overview" patch for its launcher, which includes a working example that demonstrates what you imagine to be a typical use case


- [x] add reference tab to help files
	- [x] aoo.send~
	- [x] aoo.receive~
	- [x] aoo.client
	- [x] aoo.server

#### Listing in the Package Manager, and initial impressions:


- [x] "The "About" text in the PM could be more descriptive." / "The About text doesn't really say anything"

- [x] Which versions of macOS and Windows does this package support? It currently states "all versions."

- [x] The name is written as Aoo, or aoo, or AOO

- [x] "Clicking ""Aoo Objects Overview"" in a help patch opens up a new instance of the launcher, even if it is already open. If the launcher is already open, it would be preferable to just bring it to the front instead of opening multiple instances." (could use the [onecopy] object)

#### On the need for an initial example patcher:

- [x] "I think they could use a master example patch that ties all 4 objects together in a way that demonstrates to new users what the vision of the package is. I understand them, but for example, I don't know why the client and server are separate from send~ and receive~"
- [x] "It's a simple package with only four objects, but it took me a bit of time to digest the role and need of the aoo.client and aoo.server objects. Maybe a more rich overview patch with a working example might be nice for first time users"
- [x] "A primer would be nice. I didn't know out of the box if I needed to set up the server or dive straight into send or receive."

#### Help file issues:
- [x] In the aoo.receive~ and aoo.send~ help patches, the ""Advanced"" tab needs some love. The description is one word, there are no numbered steps, and it's a bit cluttered.
- [x] Under the ""multichannel"" tab of the aoo.receive~ and aoo.send~ help patches, I don't understand what ""sink_channel"" is supposed to do. The help file has no explaination. The ref page says ""Add a channel offset to the aoo.receive~ output channels"", but that still doesn't make sense to me. A better description in both the ref and help would be useful.
- [x] aoo.client: typo on the ""messaging"" tab, ""Clients may sent"". Under the ""example"" tab, there are multiple steps to do in step 1 and step 2, but they are not labeled properly. If you miss the 2) below, the patch won't work. All steps should be in bubble comments. And lastly, it would be much more engaging to have actual sound in this help patch.
- [x] "I can't get the example in the "example" tab of aoo.client to work. I follow the numbered steps, but get "aoo.send~: couldn't find sink test|bar 1""
- [x] "I also can't get the example in aoo.client messaging tab to work by following the steps--nothing happens (closed and reopened the patch, restarted Max)"
- [x] aoo.server: Again, there is a second step, under step 1. This should just be labeled separately with a bubble comment. When clicing the ""group_join test foo"" message, I get the error ""aoo.client: too few arguments for 'group_join' method"". Under the ""Example"" tab steps aren't labeled properly again. I can't get audio to come out of aoo.receive~. When clicking ""invite test foo 1"", I get the error ""aoo.receive~: couldn't find source test|foo 1"". "

#### 
- [x] The advanced tab for aoo.receive~ says "see reference for all messages", but I don't know where to find the reference
- [x] The @multichannel option is nice. It should probably take an argument (0/1) to be consistent with common attribute usage
- [x] Nitpick: in aoo.client, in the comments in the example tab, they write "Use group|user (instead of IP|port)…", and I find the use of the pipe "|" unnecessarily confusing since you don't actually use it in the message you send to the object (you separate group and user with a space).
- [x] Needs proofreeding for typos (aoo.client.maxhelp, messaging tab: "Clients may sent messages…")


- [x] Check the latency message...whether it is in seconds or milliseconds