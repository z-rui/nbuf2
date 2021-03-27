@0xfc3a5e18c1ae8543;

struct Root {
	entries @0 :List(Entry);
}

struct Entry {
	magic @0 :UInt64;
	id @1 :Int32;
	pi @2 :Float64;
	coordinates @3 :List(Float32);
	msg @4 :Text;
}
