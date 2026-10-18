# The original disc

`rhft.zip` is the 1996 distribution exactly as archived. Despite the extension it is a
David Pilling **Spark** archive, not PKZIP, so `unzip` will not open it. On Linux:

    sudo apt install nspark
    mkdir extracted && cd extracted && nspark -x ../original/rhft.zip

Two files inside have `?` in their names (`!RHFT/Maps/SwitchWay?`, `!RHFT/Sounds/ready?`),
which Windows cannot create, so the unpacked tree is deliberately not committed. The build
uses `rhft.pack` (all of `!RHFT` bundled with RISC OS filetypes) instead; regenerate it with
`python3 tools/mkpack.py extracted rhft.pack listing.txt` after unpacking, and `listing.txt`
with `nspark -l -v original/rhft.zip > listing.txt`.
