zsh_directory_name() {
	setopt extendedglob
	if [[ $1 == n ]]; then
		reply=("$KODOKU_HOME/$2")
	elif [[ $1 == d ]]; then
		[[ $2 == (#b)($KODOKU_HOME/([^/]##))* ]] && reply=($match[2] $#match[1])
	elif [[ $1 == c ]]; then
		dirs=($KODOKU_HOME/*(/:t))
		_wanted dynamic-dirs expl 'kodoku home' compadd -S\] -a dirs
	else
		return 1
	fi
}
