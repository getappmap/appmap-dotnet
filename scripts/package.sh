#/bin/bash -e

cd "$( dirname "${BASH_SOURCE[0]}" )"/..

scripts/get-clrie.sh Darwin
scripts/get-clrie.sh Linux

chmod +x bin/*/*

dotnet pack launcher -c Release
