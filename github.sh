#/bin/bash
export PATH=$PATH
echo $PATH
sudo git add .
sudo git commit -m "`date "+%Y-%m-%d %H:%M:%S"`"
sudo git push https://github.com/ffteng46/ctp_hft.git
