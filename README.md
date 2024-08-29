# PixelIt.WebUI

This is the device WebUI for the awesome [PixelIt Project](https://github.com/pixelit-project/PixelIt).

## How to Develop?

1. **Clone the Repository**
   - Clone the project repository to your local machine:
     ```bash
     git clone https://github.com/pixelit-project/PixelIt.WebUI.git
     ```

2. **Navigate to the Project Directory**
   - Change into the project directory:
     ```bash
     cd PixelIt.WebUI
     ```

3. **Install Dependencies**
   - Install the necessary dependencies using `npm`:
     ```bash
     npm install
     ```

4. **Set Up Environment Variables**
   - Copy the example environment file to a new `.env.local` file:
     ```bash
     cp .env.local.example .env.local
     ```
   - On Windows, use:
     ```bash
     copy .env.local.example .env.local
     ```
   - Edit the `.env.local` file to suit your specific needs.

5. **Start the Development Server**
   - Run the following command to start the development server:
     ```bash
     npm run serve
     ```
   - On Windows, if applicable, use:
     ```bash
     npm run serve-win
     ```
   - The server will start on port `8080`. You can access it at `http://localhost:8080`.
