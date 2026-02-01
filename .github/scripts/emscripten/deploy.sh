#!/bin/bash
# Deploy Emscripten build to server via SSH
# Expects environment variables: SSH_PRIVATE_KEY, SERVER_IP, SERVER_USER, SERVER_PATH_EMSCRIPTEN_NIGHTLY
set -e

echo "Setting up SSH..."
mkdir -p ~/.ssh
echo "$SSH_PRIVATE_KEY" > ~/.ssh/deploy_key
chmod 600 ~/.ssh/deploy_key

# Add server to known_hosts to avoid interactive prompt
echo "Adding server to known_hosts..."
ssh-keyscan -p 22033 -H "$SERVER_IP" >> ~/.ssh/known_hosts

# Deploy using rsync over SSH
echo "Deploying to server..."
scp -P 22033 -i ~/.ssh/deploy_key -r deploy/* "$SERVER_USER@$SERVER_IP:$SERVER_PATH_EMSCRIPTEN_NIGHTLY"

# Clean up
rm -f ~/.ssh/deploy_key

echo "Deployment complete!"
