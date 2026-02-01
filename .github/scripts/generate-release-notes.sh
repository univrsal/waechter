#!/bin/bash
# Generate release notes
set -e

TAG_NAME="$1"

echo "Generating release notes..."

cat > release_notes.md << EOF
## Waechter Release ${TAG_NAME}

**Note:** This is a draft release. Please test before marking as final.

### SHA256 Checksums

\`\`\`
EOF

cat checksums.txt >> release_notes.md

cat >> release_notes.md << 'EOF'
```
EOF

echo "Release notes generated!"
cat release_notes.md
