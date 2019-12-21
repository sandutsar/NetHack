

des.level_init({ style = "solidfill", fg = " " });
des.level_flags("mazelevel");

des.map([[
.......................................................................
.......................................................................
.......................................................................
.......................................................................
...C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C...
...CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC...
...C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C...
...CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC...
...C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C...
...CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC...
...C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C...
...CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC...
...C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C...
...CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC...
...C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C C...
.......................................................................
.......................................................................
.......................................................................
.......................................................................
]]);

if math.random(0,99) < 40 then
   -- occasionally it's not a fog maze
   local terrain = { "L", "}", "T", "-", "F" };
   local tidx = math.random(1, #terrain);
   -- break it up a bit
   des.replace_terrain({ region={0, 0, 70, 18}, fromterrain="C", toterrain=".", chance=5 });
   des.replace_terrain({ region={0, 0, 70, 18}, fromterrain="C", toterrain=terrain[tidx] });
end;

des.region(selection.area(00,00,70,18), "lit");

-- when falling down on this level, never end up in the fog maze
des.teleport_region({ region = {00,00,70,18}, exclude = {02,03,68,15}, dir = "down" });

for i = 1,15 do
   des.object();
end

for i = 1,6 do
   des.trap();
end

for i = 1,28 do
  des.monster();
end

des.mazewalk({ x=4, y=2, dir="south", stocked=0 });

-- Stairs up, not in the fog maze
des.levregion({ region = {00,00,70,18}, exclude = {02,03,68,15}, type="stair-up"});
des.stair("down");
