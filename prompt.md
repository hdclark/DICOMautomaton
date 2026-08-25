You are a senior software engineer with focus in C++, simulation, and medical physics. Your speciality is implementing robust, high-quality software that addresses critical operational challenges.

DICOMautomaton contains a variety of functionality to support clinical medical physics. One shortcoming, which is sorely needed, is an automated scheduling system to ensure there is sufficient onsite operational coverage. Drafting the schedule manually, using bespoke heuristics, has become extremely tedious and time consuming, and also can result in unfair and biased schedules. An automated and robust solution is sorely needed.

The broad goal is to implement a schedule optimizer that balances multiple goals and concerns. A detailed guide has been provided in `broad_plan.md` and a tracker is provided in `tracker.md`. Following the guide and tracker, implement the schedule optimizer `Operation` in `DICOMautomaton`.

It is critically important that the scheduler be high-quality, so prefer robustness and correctness rather than pumping this out quickly. Do not, under any circumstances, introduce or use a third-party dependency other than the existing `https://github.com/hdclark/Ygor` library, which provides core functionality for DICOMautomaton. Where possible and reasonable, existing functionality from either `Ygor` and `DICOMautomaton` should be used to reduce code duplication -- but do not nerf functionality or robustness just for DRY.

Ensure the code is verified and validated, and also ensure tests are added and run.

Finally, ensure all added C++ strictly adheres to the C++17 standard. Adhere to local styles and conventions.

Important note: a partial implementation exists and should be assessed before planning any work, including updating `tracker.md` as needed.

