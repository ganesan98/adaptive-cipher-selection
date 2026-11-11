# Publish this reconstruction

Suggested repository name: `adaptive-cipher-selection`.

The project has no embedded credentials. `*.key`, `runs/`, virtual environments,
and builds are ignored. Review `git status` before publishing.

With GitHub CLI installed and authenticated, run from the project root:

```bash
gh auth login
git init -b main
git add .
git commit -m "Build adaptive UDP cipher selection prototype"
gh repo create adaptive-cipher-selection --public --source . --remote origin --push
```

If this folder already has a Git repository and commit, skip `git init`,
`git add`, and `git commit` as appropriate. Use `--private` if you want a private
repository. Do not force-push over an existing repository.

If GitHub CLI is unavailable, create an **empty** GitHub repository in your
account (do not initialize a README), then:

```bash
git remote add origin https://github.com/YOUR_USERNAME/adaptive-cipher-selection.git
git push -u origin main
```

Replace `YOUR_USERNAME` with your account. If origin already exists, inspect
`git remote -v` before changing anything. Check the Actions tab after the push
and resolve any failures before presenting the project as fully tested.

This is new code with new history. Do not backdate commits or imply the deleted
repository was recovered.
