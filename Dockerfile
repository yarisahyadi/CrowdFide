# Use a Node.js base image with version 14.x
FROM node:14

# Set the working directory in the container
WORKDIR /app

# Copy the package.json and package-lock.json files into the container
COPY package*.json ./

# Install the dependencies in the container
RUN npm install

# Copy the application code into the container
COPY . .

# Expose port 8081 for the application to listen on
EXPOSE 8081

# Start the application when the container starts
CMD ["npm", "start"]
