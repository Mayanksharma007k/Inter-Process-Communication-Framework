/** @type {import('tailwindcss').Config} */
export default {
  darkMode: 'class',
  content: ["./index.html", "./src/**/*.{js,ts,jsx,tsx}"],
  theme: {
    extend: {
      colors: {
        discord: {
          blurple: '#5865F2',
          green: '#57F287',
          yellow: '#FEE75C',
          fuchsia: '#EB459E',
          red: '#ED4245',
          dark: {
            serverNav: '#1E1F22',
            channelNav: '#2B2D31',
            chatBg: '#313338',
            chatCard: '#2B2D31',
            memberNav: '#2B2D31',
            textNormal: '#DBDEE1',
            textMuted: '#949BA4',
            textHeader: '#F2F3F5',
            hover: '#35373C',
            active: '#404249',
            divider: '#35363C',
          },
          light: {
            serverNav: '#E3E5E8',
            channelNav: '#F2F3F5',
            chatBg: '#FFFFFF',
            chatCard: '#F2F3F5',
            memberNav: '#F2F3F5',
            textNormal: '#313338',
            textMuted: '#5C5E66',
            textHeader: '#060607',
            hover: '#E0E2E5',
            active: '#D4D7DC',
            divider: '#E3E5E8',
          }
        }
      }
    },
  },
  plugins: [],
}
