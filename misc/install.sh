cd /tmp
echo "Downloading bootstrap compiler sources..."
wget https://github.com/mask-lang/mask/releases/download/v0.1.0/mask-0.1.0.tar.bz2

echo "Done. Extracting binaries"

tar zxf mask-0.1.0.tar.bz2
cp mask-0.1.0/* /usr/local/bin

echo "Done. You're good to go"
