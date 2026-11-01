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

## Firmware release downloads

The update page reuses `/api/Releases` data from PixelIt.API. Direct firmware
links require `fwdownloads` entries containing `fileName` (original asset name)
and `downloadURL` (browser download URL). Deploy the corresponding PixelIt.API
extension before this WebUI. With older API deployments or unknown hardware,
the page shows release notes instead of guessing a firmware URL or querying the
GitHub API directly. Full-upgrade images are excluded from OTA download selection.

## Reporting gallery icons

The flag button in the gallery opens a report dialog. Users select a reason and may add details; details are
required for "Other reason". The combined reason and details (up to 1000 characters)
are sent to PixelIt.API's `POST /api/reports`; no login is required.
A moderator decides whether to hide the icon. Reports do not delete it immediately.
Deploy the API's moderation endpoints before this WebUI. Gallery reads and reports
use the configured `VUE_APP_API_SERVER` base URL. Reporting is disabled in demo mode.
No report requests are sent to the display and no background polling is added.
