let _updateTime = 0;

            Object.defineProperty(Module, "hasUpdate", {
              get: function() {
                return !!_updateTime;
              },
              set: function(value) {
                if (value) _updateTime++;
                else _updateTime = 0;

                // pre 5 times syncfs
                if (value % 5 === 0) {
                  Module.syncfs();
                }
              }
            });
            Module.syncfs = function() {
              FS.syncfs(false, function(err) {
                if (err) {
                  console.error(err);
                }
                Module.hasUpdate = false;
              });
            }

            window.onbeforeunload = function(e) {
              Module.syncfs();
              if (Module.hasUpdate) {
                e.preventDefault();
                e.returnValue = "Not saved yet!";
              }
            }
            
            
            Module.inited = new Promise(function(resolve, reject) {
                Module.onRuntimeInitialized = function() {
                    FS.mkdir('/data');
                    FS.mount(IDBFS, {}, '/data');
                    FS.syncfs(true, function (err) {
                        if (err) {
                            console.error(err);
                        }
                        resolve(true);
                    });
                }
                
                Module.onAbort = function() {
                    reject(false);
                }
            });
