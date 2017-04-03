require "openssl" # for OpenSSL::Random.random_bytes

SMALLSIZE = 400
BIGSIZE = 12000000

SAMPLES = {
  "empty"               => "",
  "\\0 (small size)"    => "\0".b * SMALLSIZE,
  "\\0 (big size)"      => "\0".b * BIGSIZE,
  "\\xaa (small size)"  => "\xaa".b * SMALLSIZE,
  "\\xaa (big size)"    => "\xaa".b * BIGSIZE,
  "random (small size)" => OpenSSL::Random.random_bytes(SMALLSIZE),
  "random (big size)"   => OpenSSL::Random.random_bytes(BIGSIZE),
}

SAMPLES["freebsd ports index"] = File.read("/usr/ports/INDEX-10", mode: "rb") rescue nil # if on FreeBSD
SAMPLES["freebsd kernel"] = File.read("/boot/kernel/kernel", mode: "rb") rescue nil # if on FreeBSD
